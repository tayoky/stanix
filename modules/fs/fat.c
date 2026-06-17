#include <kernel/cache.h>
#include <kernel/kheap.h>
#include <kernel/module.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/unicode.h>
#include <kernel/vfs.h>
#include <module/fat.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

#undef min
#define min(a, b) (a < b ? a : b)

static vfs_inode_ops_t fat_inode_ops;
static vfs_fd_ops_t fat_fd_ops;
static slab_cache_t fat_inodes_slab;

static size_t fat_cluster2offset(fat_superblock_t *fat_superblock, uint32_t cluster) {
	return cluster * fat_superblock->cluster_size + fat_superblock->data_start;
}

static uint32_t fat_get_next_cluster(fat_superblock_t *fat_superblock, uint32_t cluster) {
	switch (fat_superblock->fat_type) {
	case FAT12:;
		off_t offset12 = fat_superblock->reserved_sectors * fat_superblock->sector_size + (cluster * 3) / 2;
		uint8_t ent12[3];
		vfs_read(fat_superblock->superblock.device, &ent12, offset12, sizeof(ent12));
		uint16_t ent;
		if (cluster % 2) {
			ent = (ent12[1] >> 4) | (ent12[2] << 4);
		} else {
			ent = ent12[0] | ((ent12[1] & 0x0F) << 8);
		}
		if (ent >= 0xFF8) return FAT_EOF;
		return ent;
	case FAT16:;
		uint16_t ent16;
		off_t offset16 = fat_superblock->reserved_sectors * fat_superblock->sector_size + cluster * 2;
		vfs_read(fat_superblock->superblock.device, &ent16, offset16, sizeof(ent16));
		if (ent16 >= 0xFFF8) return FAT_EOF;
		return ent16;
	case FAT32:;
		uint32_t ent32;
		off_t offset32 = fat_superblock->reserved_sectors * fat_superblock->sector_size + cluster * 4;
		vfs_read(fat_superblock->superblock.device, &ent32, offset32, sizeof(ent32));
		if (ent32 >= 0x0FFFFFF8) return FAT_EOF;
		return ent32 & 0x0FFFFFFF;
	default:
		kassert(!"invalid fat type");
		return FAT_EOF;
	}
}

static int fat_read_pages(cache_t *cache, off_t offset, size_t size) {
	fat_inode_t *inode               = container_of(cache, fat_inode_t, cache);
	fat_superblock_t *fat_superblock = container_of(inode->vnode.superblock, fat_superblock_t, superblock);
	// cluster size is always divier or multiple of page size

	// start by going to the first cluster
	uint32_t start_cluster = offset / fat_superblock->cluster_size;
	uint32_t cluster       = inode->first_cluster;
	for (size_t i = 0; i < start_cluster; i++) {
		// early EOF ??? probably corrupted fat fs
		if (cluster == FAT_EOF) return -EIO;
		cluster = fat_get_next_cluster(fat_superblock, cluster);
	}

	// we got the first cluster
	// read page per page
	size_t cluster_offset = offset % fat_superblock->cluster_size; // offset within the current cluster
	for (uintptr_t addr = offset; addr < offset + size; addr += PAGE_SIZE) {
		uintptr_t page = cache_get_page(cache, addr);
		kassert(page != PAGE_INVALID);
		char *vaddr = mmu_phys2virt(page);
		for (size_t count = 0; count < PAGE_SIZE;) {
			size_t read_size = min(PAGE_SIZE, fat_superblock->cluster_size - cluster_offset);

			// early EOF ??? probably corrupted fat fs
			if (cluster == FAT_EOF) return -EIO;

			ssize_t ret = vfs_read(fat_superblock->superblock.device, vaddr, fat_cluster2offset(fat_superblock, cluster) + cluster_offset, read_size);
			if (ret < 0) return (int)ret;
			if (ret != (ssize_t)read_size) return -EIO;

			vaddr += read_size;
			cluster_offset += read_size;
			count += read_size;
			if (cluster_offset == fat_superblock->cluster_size) {
				cluster_offset = 0;
				cluster        = fat_get_next_cluster(fat_superblock, cluster);
			}
		}
	}
	cache_read_terminate(cache, offset, size);
	return 0;
}

static cache_ops_t fat_cache_ops = {
	.read = fat_read_pages,
};

static vfs_node_t *fat_entry2node(fat_entry_t *entry, fat_superblock_t *fat_superblock) {
	fat_inode_t *inode   = slab_alloc(&fat_inodes_slab);
	inode->entry         = *entry;
	inode->first_cluster = (entry->cluster_higher << 16) | entry->cluster_lower;

	inode->vnode.ops        = &fat_inode_ops;
	inode->vnode.superblock = &fat_superblock->superblock;
	inode->vnode.ref_count  = 1;
	inode->vnode.number     = inode->first_cluster;
	if (entry->attribute & ATTR_DIRECTORY) {
		inode->vnode.mode = S_IFDIR | 0777;
	} else {
		inode->vnode.mode = S_IFREG | 0777;
		init_cache(&inode->cache);
		inode->cache.ops  = &fat_cache_ops;
		inode->cache.size = entry->file_size;
	}
	return &inode->vnode;
}

static ssize_t fat_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	fat_inode_t *inode = fd->private;
	return cache_read(&inode->cache, buffer, offset, count);
}

static ssize_t fat_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	fat_inode_t *inode = fd->private;
	return cache_write(&inode->cache, buffer, offset, count);
}

static vfs_fd_ops_t fat_fd_ops = {
	.read  = fat_read,
	.write = fat_write,
};

static int fat_open(vfs_fd_t *fd) {
	fat_inode_t *inode = container_of(fd->inode, fat_inode_t, vnode);
	fd->ops            = &fat_fd_ops;
	fd->private        = inode;
	return 0;
}

static time_t fat_date2time(uint16_t data) {
	int day   = data & 0x1f;
	int month = (data >> 5) & 0xf;
	int year  = ((data >> 9) & 0x7f) + 1980;
	return date2time(year, month, day, 0, 0, 0);
}

static int fat_getattr(vfs_node_t *vnode, struct stat *st) {
	fat_inode_t *inode = container_of(vnode, fat_inode_t, vnode);
	// no meta data on root (emulated on fat 32 root)
	if (inode->is_fat16_root) return 0;

	// TODO : parse times
	st->st_size  = inode->entry.file_size;
	st->st_atime = fat_date2time(inode->entry.access_date);
	st->st_mtime = fat_date2time(inode->entry.write_date);
	// technicly the ctime is not creation but change, but fat does not have ctime
	st->st_ctime = fat_date2time(inode->entry.creation_date);
	return 0;
}

static int fat_read_entry(fat_superblock_t *fat_superblock, size_t offset, fat_entry_t *entry) {
	return vfs_read(fat_superblock->superblock.device, entry, offset, sizeof(fat_entry_t));
}

static int fat_next_entry(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t *cluster, size_t *offset, fat_entry_t *entry) {
	if (*cluster == FAT_EOF) return -ENOENT;
	int ret = fat_read_entry(fat_superblock, *offset, entry);
	if (ret < 0) return ret;

	// jump to next entry
	*offset += sizeof(fat_entry_t);
	if (!inode->is_fat16_root && !((*offset) % fat_superblock->cluster_size)) {
		// end of cluster
		// jump to next cluster
		*cluster = fat_get_next_cluster(fat_superblock, *cluster);
		if (*cluster != FAT_EOF) {
			*offset = fat_cluster2offset(fat_superblock, *cluster);
		}
	}
	return 0;
}

/**
 * @brief parse fat entries and make a dirent from it
 */
static int fat2dirent(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t cluster, size_t offset, struct dirent *dirent) {
	fat_entry_t entry;
	int ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, &entry);
	if (ret < 0) return ret;

	if (entry.name[0] == 0x00) {
		// everything is free after that
		// we hit last
		return -ENOENT;
	}

	if ((entry.attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
		kdebugf("long name\n");
		// we have a long name
		fat_long_entry_t long_entry;
		memcpy(&long_entry, &entry, sizeof(fat_entry_t));

		// the first entry must have the last flag
		// because entries are stored in reverse order
		if (!(long_entry.ord & LAST_LONG_ENTRY)) return -EIO;
		uint16_t name[256];
		size_t name_len = 0;
		for (size_t ord = (long_entry.ord & ~LAST_LONG_ENTRY); ord > 0; ord--) {
			if ((long_entry.ord & ~LAST_LONG_ENTRY) != ord) {
				// corrupted
				return -EIO;
			}
			if ((long_entry.attribute & ATTR_LONG_NAME) != ATTR_LONG_NAME) {
				// corrupted
				return -EIO;
			}

			// append name
			size_t i = (ord - 1) * 13;
			memcpy(&name[i], long_entry.name1, sizeof(long_entry.name1));
			memcpy(&name[i + 5], long_entry.name2, sizeof(long_entry.name2));
			memcpy(&name[i + 11], long_entry.name3, sizeof(long_entry.name3));
			name_len += 13;

			ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, (fat_entry_t *)&long_entry);
			if (ret < 0) return ret;
		}

		// convert name to utf8
		ssize_t len = utf16_to_utf8(name, name_len, (uint8_t *)dirent->d_name);
		if (len < 0) return len;
		dirent->d_name[len] = '\0';
		memcpy(&entry, &long_entry, sizeof(fat_entry_t));
	} else {
		size_t j = 0;
		for (int i = 0; i < 8; i++) {
			if (entry.name[i] == ' ') break;
			if (entry.nt_reserved & FAT_NT_CASE_LOWER_BASE) {
				dirent->d_name[j++] = tolower(entry.name[i]);
			} else {
				dirent->d_name[j++] = toupper(entry.name[i]);
			}
		}
		// don't add . for directories/files without extention
		if (entry.name[8] != ' ') {
			dirent->d_name[j++] = '.';
		}
		for (int i = 8; i < 11; i++) {
			if (entry.name[i] == ' ') break;
			if (entry.nt_reserved & FAT_NT_CASE_LOWER_BASE) {
				dirent->d_name[j++] = tolower(entry.name[i]);
			} else {
				dirent->d_name[j++] = toupper(entry.name[i]);
			}
		}
		dirent->d_name[j] = '\0';
	}

	if (entry.attribute & ATTR_DIRECTORY) {
		dirent->d_type = DT_DIR;
	} else {
		dirent->d_type = DT_REG;
	}
	return 0;
}

static int fat_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	fat_inode_t *inode               = container_of(vnode, fat_inode_t, vnode);
	fat_superblock_t *fat_superblock = container_of(vnode->superblock, fat_superblock_t, superblock);

	size_t offset     = inode->is_fat16_root ? inode->start : fat_cluster2offset(fat_superblock, inode->first_cluster);
	uint32_t remaning = inode->is_fat16_root ? inode->entries_count : UINT32_MAX;
	uint32_t cluster  = inode->first_cluster;
	kdebugf("readdir on %s , first cluster is %lx\n", inode->is_fat16_root ? "root" : "not root", cluster);
	while (index > 0) {
		if (remaning == 0) return -ENOENT;
		// skip everything with VOLUME_ID attr or free
		for (;;) {
			fat_entry_t entry;
			int ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, &entry);
			if (ret < 0) return ret;
			if (entry.name[0] == 0x00) {
				// everything is free after that
				// we hit last
				return -ENOENT;
			}
			if (!(entry.attribute & ATTR_VOLUME_ID) && (entry.name[0] != (char)0xe5)) break;
			remaning--;
			if (remaning == 0) return -ENOENT;
		}
		index--;
		remaning--;
	}
	if (remaning == 0) return -ENOENT;

	return fat2dirent(fat_superblock, inode, cluster, offset, dirent);
}

static int fat_entry_match(fat_entry_t *entry, const char *name) {
	// note that this matching function is case non sensitive
	size_t j = 0;
	for (int i = 0; i < 8; i++) {
		if (entry->name[i] == ' ') break;
		// broken entry check
		if (entry->name[i] < 0x20) return 0;
		if (toupper(name[j++]) != entry->name[i]) return 0;
	}
	if (name[j] == '.') j++;
	for (int i = 8; i < 11; i++) {
		if (entry->name[i] == ' ') break;
		// broken entry check
		if (entry->name[i] < 0x20) return 0;
		if (toupper(name[j++]) != entry->name[i]) return 0;
	}
	if (name[j]) return 0;
	return 1;
}

static int fat_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	fat_inode_t *inode               = container_of(vnode, fat_inode_t, vnode);
	fat_superblock_t *fat_superblock = container_of(vnode->superblock, fat_superblock_t, superblock);

	uint32_t remaning = inode->is_fat16_root ? inode->entries_count : UINT32_MAX;
	uint64_t offset   = inode->is_fat16_root ? inode->start : fat_cluster2offset(fat_superblock, inode->first_cluster);
	uint32_t cluster  = inode->first_cluster;

	while (remaning > 0) {
		fat_entry_t entry;
		int ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, &entry);
		if (ret < 0) return ret;
		if (inode->is_fat16_root) remaning--;

		if (entry.name[0] == 0x00) {
			// everything is free after that
			// we hit last
			return -ENOENT;
		}

		if (entry.name[0] == (char)0xe5) {
			// free entry
			continue;
		}

		if ((entry.attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
			// long name
			fat_long_entry_t long_entry;
			memcpy(&long_entry, &entry, sizeof(fat_entry_t));

			// the first entry must have the last flag
			// because entries are stored in reverse order
			if (!(long_entry.ord & LAST_LONG_ENTRY)) return -EIO;
			uint16_t name[256];
			size_t name_len = 0;
			for (size_t ord = (long_entry.ord & ~LAST_LONG_ENTRY); ord > 0; ord--) {
				if ((long_entry.ord & ~LAST_LONG_ENTRY) != ord) {
					// corrupted
					return -EIO;
				}
				if ((long_entry.attribute & ATTR_LONG_NAME) != ATTR_LONG_NAME) {
					// corrupted
					return -EIO;
				}

				// append name
				size_t i = (ord - 1) * 13;
				memcpy(&name[i], long_entry.name1, sizeof(long_entry.name1));
				memcpy(&name[i + 5], long_entry.name2, sizeof(long_entry.name2));
				memcpy(&name[i + 11], long_entry.name3, sizeof(long_entry.name3));
				name_len += 13;

				ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, (fat_entry_t *)&long_entry);
				if (ret < 0) return ret;
			}
			memcpy(&entry, &long_entry, sizeof(fat_entry_t));

			// convert name to utf8
			char utf8_name[512];
			ssize_t len = utf16_to_utf8(name, name_len, (uint8_t *)utf8_name);
			if (len < 0) return len;
			utf8_name[len] = '\0';
			if (!strcmp(dentry->name, utf8_name)) {
				// we found it
				dentry->inode = fat_entry2node(&entry, fat_superblock);
				// TODO : inode number
				return 0;
			}
		} else {
			// we need to ignore volume id entries
			if (entry.attribute & ATTR_VOLUME_ID) continue;
		}

		if (fat_entry_match(&entry, dentry->name)) {
			// we found it
			dentry->inode = fat_entry2node(&entry, fat_superblock);
			// TODO : inode number
			return 0;
		}
	}

	return -ENOENT;
}

static void fat_cleanup(vfs_node_t *vnode) {
	slab_free(vnode);
}

int fat_mount(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)flags;
	(void)data;
	(void)target;
	vfs_fd_t *dev = vfs_open(source, O_RDONLY);
	if (!dev) return -ENOENT;

	fat_bpb bpb;
	if (vfs_read(dev, &bpb, 0, sizeof(bpb)) != sizeof(bpb)) {
		vfs_close(dev);
		return -EIO; // not sure this is the good error code
	}
	kdebugf("%ld\n", offsetof(fat_bpb, extended.fat32.signature));
	if (bpb.extended.fat32.signature != 0xaa55) {
		kdebugf("invalid signature\n");
		vfs_close(dev);
		return -EFTYPE;
	}

	kdebugf("oem is '%s'\n", bpb.oem_name);

	if (bpb.byte_per_sector < 512 || bpb.sector_per_cluster == 0) {
		kdebugf("invalid byte per sectors / sectors per cluster\n");
		vfs_close(dev);
		return -EFTYPE;
	}

	uint16_t root_sectors    = ((bpb.root_entires_count * 32) + bpb.byte_per_sector - 1) / bpb.byte_per_sector;
	uint32_t sectors_per_fat = bpb.sectors_per_fat16 ? bpb.sectors_per_fat16 : bpb.extended.fat32.sectors_per_fat32;
	uint32_t sectors_count   = bpb.sectors_count16 ? bpb.sectors_count16 : bpb.sectors_count32;
	uint32_t data_sectors    = sectors_count - (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat + root_sectors);
	uint32_t data_clusters   = data_sectors / bpb.sector_per_cluster;

	int fat_type;
	if (data_clusters < 4085) {
		fat_type = FAT12;
		kdebugf("fat12\n");
	} else if (data_clusters < 65525) {
		fat_type = FAT16;
		kdebugf("fat16\n");
	} else {
		fat_type = FAT32;
		kdebugf("fat32\n");
	}

	if (fat_type == FAT32) {
		if (bpb.extended.fat32.version != 0) {
			vfs_close(dev);
			return -EFTYPE;
		}
	} else {
		// TODO : fat12/16 check
	}

	fat_superblock_t *fat_superblock = kmalloc(sizeof(fat_superblock_t));
	memset(fat_superblock, 0, sizeof(fat_superblock_t));
	fat_superblock->superblock.device = dev;
	fat_superblock->fat_type          = fat_type;
	fat_superblock->reserved_sectors  = bpb.reserved_sectors;
	fat_superblock->sector_size       = bpb.byte_per_sector;
	fat_superblock->cluster_size      = bpb.byte_per_sector * bpb.sector_per_cluster;
	fat_superblock->data_start        = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector;

	vfs_node_t *local_root;
	if (fat_type == FAT32) {
		// build a fake entry for root
		fat_entry_t root_entry;
		memset(&root_entry, 0, sizeof(root_entry));
		root_entry.attribute      = ATTR_DIRECTORY;
		root_entry.cluster_lower  = bpb.extended.fat32.root_cluster & 0xf;
		root_entry.cluster_higher = (bpb.extended.fat32.root_cluster >> 8) & 0xf;
		// how do we get size ?
		local_root = fat_entry2node(&root_entry, fat_superblock);
	} else {
		// root of fat12/16 is really stupid
		fat_inode_t *root   = slab_alloc(&fat_inodes_slab);
		root->entries_count = bpb.root_entires_count;
		root->start         = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector;
		root->is_fat16_root = 1;

		local_root             = &root->vnode;
		local_root->mode       = S_IFDIR | 0777;
		local_root->ops        = &fat_inode_ops;
		local_root->superblock = &fat_superblock->superblock;
	}
	local_root->ref_count           = 1;
	fat_superblock->superblock.root = local_root;
	*superblock_out                 = &fat_superblock->superblock;
	return 0;
}

static vfs_inode_ops_t fat_inode_ops = {
	.readdir = fat_readdir,
	.lookup  = fat_lookup,
	.getattr = fat_getattr,
	.cleanup = fat_cleanup,
	.open    = fat_open,
};

static vfs_filesystem_t fat_fs = {
	.mount = fat_mount,
	.name  = "fat",
};

static int fat_inode_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	fat_inode_t *inode = data;
	memset(inode, 0, sizeof(fat_inode_t));
	return 0;
}

int fat_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&fat_inodes_slab, sizeof(fat_inode_t), "fat-inodes");
	fat_inodes_slab.constructor = fat_inode_constructor;
	vfs_register_fs(&fat_fs);
	return 0;
}

int fat_fini() {
	// TODO : make sure no partition is mounted
	slab_destroy(&fat_inodes_slab);
	vfs_unregister_fs(&fat_fs);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = fat_init,
	.fini        = fat_fini,
	.author      = "tayoky",
	.name        = "fat",
	.description = "fat12/16/32 drivers",
	.license     = "GPL 3",
};
