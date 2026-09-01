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

static off_t fat_cluster2offset(fat_superblock_t *fat_superblock, uint32_t cluster) {
	kassert(cluster >= 2);
	return (cluster - 2) * fat_superblock->cluster_size + fat_superblock->data_start;
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

static int fat_raw_set_next_cluster(fat_superblock_t *fat_superblock, off_t offset, uint32_t cluster, uint32_t next) {
	switch (fat_superblock->fat_type) {
	case FAT12:
		if (next == FAT_EOF) next = 0xFF8;
		off_t offset12 = fat_superblock->reserved_sectors * fat_superblock->sector_size + (cluster * 3) / 2;
		uint8_t ent12[3];
		ssize_t ret = vfs_read(fat_superblock->superblock.device, &ent12, offset + offset12, sizeof(ent12));
		if (ret < 0) return ret;
		if (ret < (ssize_t)sizeof(ent12)) return -EIO;
		if (cluster % 2) {
			ent12[1] = ((next << 4) & 0xF0U) | (ent12[1] & 0x0FU);
			ent12[2] = (uint8_t)(next >> 4);
		} else {
			ent12[0] = (uint8_t)next;
			ent12[1] = (ent12[1] & 0xF0U) | ((next >> 8) & 0x0FU);
		}
		ret = vfs_write(fat_superblock->superblock.device, &ent12, offset + offset12, sizeof(ent12));
		if (ret < 0) return ret;
		if (ret < (ssize_t)sizeof(ent12)) return -EIO;
		return 0;
	case FAT16:
		if (next == FAT_EOF) next = 0xFFF8;
		uint16_t ent16 = (uint16_t)next;
		off_t offset16 = fat_superblock->reserved_sectors * fat_superblock->sector_size + cluster * 2;
		ret = vfs_write(fat_superblock->superblock.device, &ent16, offset + offset16, sizeof(ent16));
		if (ret < 0) return ret;
		if (ret < (ssize_t)sizeof(ent16)) return -EIO;
		return 0;
	case FAT32:
		if (next == FAT_EOF) next = 0x0FFFFFF8;
		uint32_t ent32 = next & 0x0FFFFFFF;
		off_t offset32 = fat_superblock->reserved_sectors * fat_superblock->sector_size + cluster * 4;
		ret = vfs_write(fat_superblock->superblock.device, &ent32, offset + offset32, sizeof(ent32));
		if (ret < 0) return ret;
		if (ret < (ssize_t)sizeof(ent32)) return -EIO;
		return 0;
	default:
		kassert(!"invalid fat type");
		return FAT_EOF;
	}
}

static int fat_set_next_cluster(fat_superblock_t *fat_superblock, uint32_t cluster, uint32_t next) {
	mutex_acquire(&fat_superblock->write_lock);
	for (size_t i = 0; i < fat_superblock->fat_count; i++) {
		size_t sector = fat_superblock->reserved_sectors + fat_superblock->sectors_per_fat * i;
		int ret = fat_raw_set_next_cluster(fat_superblock, sector * fat_superblock->sector_size, cluster, next);
		if (ret < 0) {
			mutex_release(&fat_superblock->write_lock);
			return ret;
		}
	}
	mutex_release(&fat_superblock->write_lock);
	return 0;
}

// TODO : update FSINFO on fat32
static uint32_t fat_allocate_cluster(fat_superblock_t *fat_superblock) {
	mutex_acquire(&fat_superblock->write_lock);
	uint32_t cluster = fat_superblock->cluster_search_hint;
	if (cluster > fat_superblock->data_clusters + 2 || cluster < 2) {
		cluster = 2;
	}
	while (cluster < fat_superblock->data_clusters + 2) {
		if (fat_get_next_cluster(fat_superblock, cluster) == FAT_FREE) {
			// this cluster is free
			fat_superblock->cluster_search_hint = cluster + 1;
			mutex_release(&fat_superblock->write_lock);
			return cluster;
		}
		cluster++;
	}
	fat_superblock->cluster_search_hint = fat_superblock->data_clusters + 2;
	mutex_release(&fat_superblock->write_lock);
	return FAT_EOF;
}

static int fat_free_cluster(fat_superblock_t *fat_superblock, uint32_t cluster) {
	mutex_acquire(&fat_superblock->write_lock);
	if (fat_superblock->cluster_search_hint > cluster) {
		fat_superblock->cluster_search_hint = cluster;
	}
	int ret = fat_set_next_cluster(fat_superblock, cluster, FAT_FREE);
	mutex_release(&fat_superblock->write_lock);
	return ret;
}

static uint32_t fat_get_cluster(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t number) {
	uint32_t cluster = inode->first_cluster;
	for (size_t i = 0; i < number; i++) {
		if (cluster == FAT_EOF)  return FAT_EOF;
		if (cluster == FAT_FREE) return FAT_FREE;
		cluster = fat_get_next_cluster(fat_superblock, cluster);
	}
	return cluster;
}

static int fat_transfer_pages(cache_t *cache, off_t offset, size_t size, int write) {
	fat_inode_t *inode               = container_of(cache, fat_inode_t, cache);
	fat_superblock_t *fat_superblock = container_of(inode->vnode.superblock, fat_superblock_t, superblock);
	// cluster size is always driver or multiple of page size

	// start by going to the first cluster
	uint32_t cluster = fat_get_cluster(fat_superblock, inode, offset / fat_superblock->cluster_size);
	if (cluster == FAT_EOF) {
		// early EOF ??? probably corrupted fat fs
		return -EIO;
	}

	// we got the first cluster
	// read page per page
	size_t cluster_offset = offset % fat_superblock->cluster_size; // offset within the current cluster
	for (uintptr_t addr = offset; addr < offset + size; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(cache, addr);
		kassert(page != PAGE_INVALID);
		char *vaddr = mmu_phys2virt(page);
		for (size_t count = 0; count < PAGE_SIZE;) {
			size_t chunk_size = min(PAGE_SIZE, fat_superblock->cluster_size - cluster_offset);

			if (cluster == FAT_EOF) {
				if (offset + PAGE_SIZE > inode->entry.file_size) {
					// we are on the last page, early EOF is normal
					memset(vaddr, 0, PAGE_SIZE - count);
					break;
				} else {
					// corrupt FS
					return -EIO;
				}
			}

			ssize_t ret;
			if (write) {
				ret = vfs_write(fat_superblock->superblock.device, vaddr, fat_cluster2offset(fat_superblock, cluster) + cluster_offset, chunk_size);
			} else {
				ret = vfs_read(fat_superblock->superblock.device, vaddr, fat_cluster2offset(fat_superblock, cluster) + cluster_offset, chunk_size);
			}
			if (ret < 0) return (int)ret;
			if (ret != (ssize_t)chunk_size) return -EIO;

			vaddr += chunk_size;
			cluster_offset += chunk_size;
			count += chunk_size;
			if (cluster_offset == fat_superblock->cluster_size) {
				cluster_offset = 0;
				cluster        = fat_get_next_cluster(fat_superblock, cluster);
			}
		}
	}
	return 0;
}

static int fat_read_pages(cache_t *cache, off_t offset, size_t size) {
	int ret = fat_transfer_pages(cache, offset, size, 0);
	if (ret < 0) return 0;
	cache_read_terminate(cache, offset, size, 0);
	return 0;
}

static int fat_write_pages(cache_t *cache, off_t offset, size_t size) {
	kdebugf("writing pages\n");
	int ret = fat_transfer_pages(cache, offset, size, 1);
	if (ret < 0) return 0;
	cache_write_terminate(cache, offset, size, 0);
	return 0;
}

static cache_ops_t fat_cache_ops = {
	.read  = fat_read_pages,
	.write = fat_write_pages,
};

static vfs_node_t *fat_entry2node(off_t entry_offset, fat_entry_t *entry, fat_superblock_t *fat_superblock) {
	fat_inode_t *inode   = slab_alloc(&fat_inodes_slab);
	inode->entry         = *entry;
	inode->first_cluster = ((uint32_t)entry->cluster_higher << 16) | entry->cluster_lower;
	inode->entry_offset  = entry_offset;

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
	kassert(S_ISREG(inode->vnode.mode));
	return cache_read(&inode->cache, buffer, offset, count);
}

static ssize_t fat_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	fat_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	if (offset + count > inode->cache.size) {
		int ret = vfs_truncate(fd->inode, offset + count);
		if (ret < 0) return ret;
	}
	return cache_write(&inode->cache, buffer, offset, count);
}

static int fat_flush(vfs_fd_t *fd, off_t offset, size_t count) {
	fat_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	int ret = cache_flush(&inode->cache, offset, count);
	if (ret < 0) return ret;
	// TODO : maybe do not sync the whole disk
	return vfs_flush(fd->inode->superblock->device);
}

static int fat_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	fat_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	return cache_mmap(&inode->cache, offset, seg);
}

static vfs_fd_ops_t fat_fd_ops = {
	.read     = fat_read,
	.write    = fat_write,
	.flush    = fat_flush,
	.mmap     = fat_mmap,
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
	// technically the ctime is not creation but change, but fat does not have ctime
	st->st_ctime = fat_date2time(inode->entry.creation_date);
	return 0;
}

static int fat_read_entry(fat_superblock_t *fat_superblock, off_t offset, fat_entry_t *entry) {
	ssize_t ret = vfs_read(fat_superblock->superblock.device, entry, offset, sizeof(fat_entry_t));
	if (ret < 0) return ret;
	if (ret < (ssize_t)sizeof(fat_entry_t)) return -EIO;
	return 0;
}

static int fat_next_entry(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t *cluster, off_t *offset, fat_entry_t *entry) {
	if (*cluster == FAT_EOF) return -ENOENT;
	if (inode->is_fat16_root) {
		size_t index = (*offset - inode->start) / sizeof(fat_entry_t);
		if (index >= inode->entries_count) {
			return -ENOENT;
		}
	}

	int ret = fat_read_entry(fat_superblock, *offset, entry);
	if (ret < 0) return ret;

	// jump to next entry
	*offset += sizeof(fat_entry_t);
	if (!inode->is_fat16_root) {
		off_t cluster_end = fat_cluster2offset(fat_superblock, *cluster) + fat_superblock->cluster_size;
		if (*offset >= cluster_end) {
			// end of cluster
			// jump to next cluster
			*cluster = fat_get_next_cluster(fat_superblock, *cluster);
			if (*cluster != FAT_EOF) {
				*offset = fat_cluster2offset(fat_superblock, *cluster);
			}
		}
	}
	return 0;
}

/**
 * @brief parse next lfn sequence
 */
static int fat_next_lfn(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t *cluster, off_t *offset, off_t *sfn_offset, fat_entry_t *entry, char name[512]) {
	fat_long_entry_t long_entry;
	memcpy(&long_entry, entry, sizeof(fat_entry_t));

	// we have a long name

	// the first entry must have the last flag
	// because entries are stored in reverse order
	if (!(long_entry.ord & LAST_LONG_ENTRY)) return -EIO;
	uint16_t utf16_name[256];
	size_t name_len = 0;
	size_t ord_count = (long_entry.ord & ~LAST_LONG_ENTRY);
	if (ord_count > 20) {
		// invalid to have more than 20
		return -EIO;
	}
	for (size_t ord = ord_count; ord > 0; ord--) {
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
		memcpy(&utf16_name[i], long_entry.name1, sizeof(long_entry.name1));
		memcpy(&utf16_name[i + 5], long_entry.name2, sizeof(long_entry.name2));
		memcpy(&utf16_name[i + 11], long_entry.name3, sizeof(long_entry.name3));
		name_len += 13;

		if (sfn_offset) *sfn_offset = *offset;
		int ret = fat_next_entry(fat_superblock, inode, cluster, offset, (fat_entry_t *)&long_entry);
		if (ret < 0) return ret;
	}

	// convert name to UTF-8
	ssize_t len = utf16_to_utf8(utf16_name, name_len, (uint8_t *)name);
	if (len < 0) return len;
	name[len] = '\0';

	// the last entry should be the associed sfn
	memcpy(entry, &long_entry, sizeof(fat_entry_t));
	if ((entry->attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
		// the last is not sfn
		return -EIO;
	}
	return 0;
}

/**
 * @brief parse a short filename entry
 */
static int fat_parse_sfn(fat_entry_t *entry, char name[512]) {
	size_t j = 0;
	for (int i = 0; i < 8; i++) {
		if (entry->name[i] == ' ') break;
		if (entry->nt_reserved & FAT_NT_CASE_LOWER_BASE) {
			name[j++] = tolower(entry->name[i]);
		} else {
			name[j++] = toupper(entry->name[i]);
		}
	}

	// don't add "." for directories/files without extension
	if (entry->name[8] != ' ') {
		name[j++] = '.';
	}

	for (int i = 8; i < 11; i++) {
		if (entry->name[i] == ' ') break;
		if (entry->nt_reserved & FAT_NT_CASE_LOWER_BASE) {
			name[j++] = tolower(entry->name[i]);
		} else {
			name[j++] = toupper(entry->name[i]);
		}
	}
	name[j] = '\0';
	return 0;
}

static int fat_sfn_match(fat_entry_t *entry, const char *name) {
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

/**
 * @brief parse fat entries and make a directory entry from it
 */
static int fat2dirent(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t cluster, off_t offset, fat_entry_t *entry, struct dirent *dirent) {
	if (entry->name[0] == 0x00) {
		// everything is free after that
		// we hit last
		return -ENOENT;
	}

	int ret = 0;
	char name[512];
	if ((entry->attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
		// we have a long name
		ret = fat_next_lfn(fat_superblock, inode, &cluster, &offset, NULL, entry, name);
		if (ret < 0) return ret;
	} else {
		// we have a short name
		ret = fat_parse_sfn(entry, name);
		if (ret < 0) return ret;
	}

	snprintf(dirent->d_name, sizeof(dirent->d_name), "%s", name);
	if (entry->attribute & ATTR_DIRECTORY) {
		dirent->d_type = DT_DIR;
	} else {
		dirent->d_type = DT_REG;
	}
	return 0;
}

static int fat_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	fat_inode_t *inode               = container_of(vnode, fat_inode_t, vnode);
	fat_superblock_t *fat_superblock = container_of(vnode->superblock, fat_superblock_t, superblock);

	off_t offset     = inode->is_fat16_root ? inode->start : fat_cluster2offset(fat_superblock, inode->first_cluster);
	uint32_t cluster  = inode->first_cluster;
	kdebugf("readdir on %s , first cluster is %lx\n", inode->is_fat16_root ? "root" : "not root", cluster);
	for (;;) {
		fat_entry_t entry;
		int ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, &entry);
		if (ret < 0) return ret;

		// skip everything with VOLUME_ID attr or free
		if (entry.name[0] == 0x00) {
			// everything is free after that
			// we hit last
			break;
		}
		if (entry.name[0] == (char)0xe5) {
			// free entry
			continue;
		}
		if ((entry.attribute & ATTR_VOLUME_ID) && (entry.attribute & ATTR_LONG_NAME) != ATTR_LONG_NAME) {
			// we have a real volume id
			continue;
		}

		if (index-- == 0) {
			return fat2dirent(fat_superblock, inode, cluster, offset, &entry, dirent);
		} else {
			// consume the entry
			while ((entry.attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
				ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, &entry);
				if (ret < 0) return ret;
			}
		}
	}
	return -ENOENT;
}

static int fat_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	fat_inode_t *inode               = container_of(vnode, fat_inode_t, vnode);
	fat_superblock_t *fat_superblock = container_of(vnode->superblock, fat_superblock_t, superblock);

	off_t offset   = inode->is_fat16_root ? inode->start : fat_cluster2offset(fat_superblock, inode->first_cluster);
	uint32_t cluster  = inode->first_cluster;

	for (;;) {
		fat_entry_t entry;
		off_t sfn_offset = offset;
		int ret = fat_next_entry(fat_superblock, inode, &cluster, &offset, &entry);
		if (ret < 0) return ret;

		if (entry.name[0] == 0x00) {
			// everything is free after that
			// we hit last
			break;
		}

		if (entry.name[0] == (char)0xe5) {
			// free entry
			continue;
		}

		if ((entry.attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
			// long name
			char name[512];
			ret = fat_next_lfn(fat_superblock, inode, &cluster, &offset, &sfn_offset, &entry, name);
			if (ret < 0) return ret;
			if (!strcmp(dentry->name, name)) {
				// we found it
				dentry->inode = fat_entry2node(sfn_offset, &entry, fat_superblock);
				// TODO : inode number
				return 0;
			}
		} else {
			// we need to ignore volume id entries
			if (entry.attribute & ATTR_VOLUME_ID) continue;
		}

		if (fat_sfn_match(&entry, dentry->name)) {
			// we found it
			dentry->inode = fat_entry2node(sfn_offset, &entry, fat_superblock);
			// TODO : inode number
			return 0;
		}
	}

	return -ENOENT;
}

static int fat_set_cluster(fat_superblock_t *fat_superblock, fat_inode_t *inode, uint32_t number, uint32_t cluster) {
	if (cluster > 0) {
		uint32_t prev = fat_get_cluster(fat_superblock, inode, number - 1);
		if (prev == FAT_EOF || prev == FAT_FREE) return -EIO;
		int ret = fat_set_next_cluster(fat_superblock, prev, cluster);
		if (ret < 0) return ret;
	} else {
		inode->first_cluster = cluster;
	}
	return 0;
}

static int fat_truncate(vfs_node_t *vnode, size_t size) {
	fat_inode_t *inode = container_of(vnode, fat_inode_t, vnode);
	fat_superblock_t *fat_superblock = container_of(inode->vnode.superblock, fat_superblock_t, superblock);
	size_t current_clusters_count = (inode->entry.file_size + fat_superblock->cluster_size - 1) / fat_superblock->cluster_size;
	size_t new_clusters_count = (size + fat_superblock->cluster_size - 1) / fat_superblock->cluster_size;

	if (new_clusters_count < current_clusters_count) {
		// free clusters
		uint32_t cluster = fat_get_cluster(fat_superblock, inode, new_clusters_count);
		for (size_t i = new_clusters_count; i < current_clusters_count; i++) {
			// early EOF ?? weird but we don't care
			if (cluster == FAT_EOF) break;
			uint32_t next = fat_get_next_cluster(fat_superblock, cluster);
			fat_free_cluster(fat_superblock, cluster);
			cluster = next;
		}
		int ret = fat_set_cluster(fat_superblock, inode, new_clusters_count, cluster);
		if (ret < 0) {
			// TODO : restore ?? IDK
			return ret;
		}
	} else if (new_clusters_count > current_clusters_count) {
		// allocate clusters
		uint32_t cluster = FAT_EOF;
		mutex_acquire(&fat_superblock->write_lock);
		for (size_t i = current_clusters_count; i < new_clusters_count; i++) {
			uint32_t new_cluster = fat_allocate_cluster(fat_superblock);
			if (new_cluster == FAT_EOF) {
				// TODO : free already allocated clusters
				mutex_release(&fat_superblock->write_lock);
				return -ENOSPC;
			}
			fat_set_next_cluster(fat_superblock, new_cluster, cluster);
			cluster = new_cluster;
		}
		mutex_release(&fat_superblock->write_lock);
		int ret = fat_set_cluster(fat_superblock, inode, current_clusters_count, cluster);
		if (ret < 0) {
			// TODO : free already allocated clusters
			return ret;
		}
	}

	cache_truncate(&inode->cache, size);
	inode->entry.file_size = size;
	return 0;
}

static void fat_cleanup(vfs_node_t *vnode) {
	fat_inode_t *inode = container_of(vnode, fat_inode_t, vnode);
	if (S_ISREG(inode->vnode.mode)) {
		free_cache(&inode->cache);
	}

	slab_free(inode);
}

static int fat_flush_inode(vfs_superblock_t *superblock, vfs_node_t *vnode) {
	fat_inode_t *inode = container_of(vnode, fat_inode_t, vnode);
	fat_superblock_t *fat_superblock = container_of(superblock, fat_superblock_t, superblock);
	kdebugf("writing inode\n");
	ssize_t ret = vfs_write(fat_superblock->superblock.device, &inode->entry, inode->entry_offset, sizeof(fat_entry_t));
	if (ret < 0) return ret;
	if (ret < (ssize_t)sizeof(fat_entry_t)) return -EIO;
	return 0;
}

static vfs_superblock_ops_t fat_superblock_ops = {
	.flush_inode = fat_flush_inode,
};

static int fat_probe(vfs_fd_t *source) {
	fat_bpb_t bpb;
	if (vfs_read(source, &bpb, 0, sizeof(bpb)) < (ssize_t)sizeof(bpb)) return 0;
	if (bpb.extended.fat32.signature != 0xaa55) return 0;

	if (!memcmp(bpb.extended.fat32.fs_type, "MSWIN   ", sizeof(bpb.extended.fat32.fs_type))) {
		return 1;
	} else if (!memcmp(bpb.extended.fat32.fs_type, "FAT32   ", sizeof(bpb.extended.fat32.fs_type))) {
		return 1;
	} else if (!memcmp(bpb.extended.fat16.fs_type, "MSDOS   ", sizeof(bpb.extended.fat16.fs_type))) {
		return 1;
	} else if (!memcmp(bpb.extended.fat16.fs_type, "FAT16   ", sizeof(bpb.extended.fat16.fs_type))) {
		return 1;
	} else if (!memcmp(bpb.extended.fat16.fs_type, "FAT12   ", sizeof(bpb.extended.fat16.fs_type))) {
		return 1;
	} else if (!memcmp(bpb.extended.fat16.fs_type, "FAT     ", sizeof(bpb.extended.fat16.fs_type))) {
		return 1;
	} else {
		return 0;
	}
}

static int fat_mount(vfs_fd_t *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)flags;
	(void)data;
	(void)target;
	fat_bpb_t bpb;
	if (vfs_read(source, &bpb, 0, sizeof(bpb)) != sizeof(bpb)) {
		return -EFTYPE;
	}
	if (bpb.extended.fat32.signature != 0xaa55) {
		kdebugf("invalid signature\n");
		return -EFTYPE;
	}

	kdebugf("OEM is '%s'\n", bpb.oem_name);

	if (bpb.byte_per_sector < 512 || bpb.sector_per_cluster == 0) {
		kdebugf("invalid byte per sectors / sectors per cluster\n");
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
			return -EFTYPE;
		}
	} else {
		// TODO : fat12/16 check
	}

	fat_superblock_t *fat_superblock = kmalloc(sizeof(fat_superblock_t));
	memset(fat_superblock, 0, sizeof(fat_superblock_t));
	fat_superblock->superblock.ops    = &fat_superblock_ops;
	fat_superblock->superblock.device = source;
	fat_superblock->fat_type          = fat_type;
	fat_superblock->reserved_sectors  = bpb.reserved_sectors;
	fat_superblock->sector_size       = bpb.byte_per_sector;
	fat_superblock->cluster_size      = bpb.byte_per_sector * bpb.sector_per_cluster;
	fat_superblock->data_start        = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat + root_sectors) * bpb.byte_per_sector;
	fat_superblock->data_clusters     = data_clusters;
	fat_superblock->fat_count         = bpb.fat_count;
	mutex_init(&fat_superblock->write_lock);

	vfs_node_t *local_root;
	if (fat_type == FAT32) {
		// build a fake entry for root
		fat_entry_t root_entry;
		memset(&root_entry, 0, sizeof(root_entry));
		root_entry.attribute      = ATTR_DIRECTORY;
		root_entry.cluster_lower  = bpb.extended.fat32.root_cluster & 0xffff;
		root_entry.cluster_higher = (bpb.extended.fat32.root_cluster >> 16) & 0xffff;
		// how do we get size ?
		local_root = fat_entry2node(0, &root_entry, fat_superblock);
	} else {
		// root of fat12/16 is really stupid
		fat_inode_t *root   = slab_alloc(&fat_inodes_slab);
		root->entries_count = bpb.root_entires_count;
		root->start         = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector;
		root->is_fat16_root = 1;
		root->entry_offset = 0;

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
	.readdir  = fat_readdir,
	.lookup   = fat_lookup,
	.getattr  = fat_getattr,
	.truncate = fat_truncate,
	.cleanup  = fat_cleanup,
	.open     = fat_open,
};

static vfs_filesystem_t fat_fs = {
	.probe = fat_probe,
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

int fat_fini(void) {
	int ret = vfs_unregister_fs(&fat_fs);
	if (ret < 0) return ret;
	slab_destroy(&fat_inodes_slab);
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
