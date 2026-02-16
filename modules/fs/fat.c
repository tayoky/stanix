#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/time.h>
#include <kernel/vfs.h>
#include <module/fat.h>
#include <stdint.h>
#include <errno.h>

#undef min
#define min(a,b) (a < b ? a : b)

static vfs_inode_ops_t fat_inode_ops;
static vfs_fd_ops_t fat_fd_ops;

static time_t fat_date2time(uint16_t data) {
	int day = data & 0x1f;
	int month = (data >> 5) & 0xf;
	int year = ((data >> 9) & 0x7f) + 1980;
	return date2time(year, month, day, 0, 0, 0);
}

static int fat_getattr(vfs_node_t *node, struct stat *st) {
	fat_inode_t *inode = node->private_inode;
	//no meta data on root (emulated on fat 32 root)
	if (inode->is_fat16_root)return 0;

	//TODO : parse times
	st->st_ino  = inode->first_cluster;
	st->st_size = inode->entry.file_size;
	st->st_atime = fat_date2time(inode->entry.access_date);
	st->st_mtime = fat_date2time(inode->entry.write_date);
	// technicly the ctime is not creation but change, but fat does not have ctime
	st->st_ctime = fat_date2time(inode->entry.creation_date);
	st->st_mode = 0777;
	return 0;
}

static size_t fat_cluster2offset(fat_superblock_t *fat_superblock, uint32_t cluster) {
	return cluster * fat_superblock->cluster_size + fat_superblock->data_start;
}

static uint32_t fat_get_next_cluster(fat_superblock_t *fat_superblock, uint32_t cluster) {
	//check oob first
	size_t ent_size; //one unit = half a byte
	switch (fat_superblock->fat_type) {
	case FAT12:
		ent_size = 3;
		break;
	case FAT16:
		ent_size = 4;
		break;
	case FAT32:
		ent_size = 8;
		break;
	}
	if (cluster > fat_superblock->sectors_per_fat * fat_superblock->sector_size * 2 / ent_size)return FAT_EOF;
	off_t offset = fat_superblock->reserved_sectors * fat_superblock->sector_size + cluster * ent_size / 2;
	uint32_t ent;
	switch (fat_superblock->fat_type) {
	case FAT12:;
		uint16_t ent16;
		vfs_read(fat_superblock->superblock.device, &ent16, offset, sizeof(ent16));
		ent = ent16 >> (cluster % 2) * 8; //not sure might be the inverse
		if (ent == 0xFFF)ent = 0xFFFFFFFF;
		break;
	case FAT16:
		vfs_read(fat_superblock->superblock.device, &ent16, offset, sizeof(ent16));
		ent = ent16;
		if (ent == 0xFFFF)ent = 0xFFFFFFFF;
		break;
	case FAT32:
		vfs_read(fat_superblock->superblock.device, &ent, offset, sizeof(ent));
		break;
	}
	ent &= 0xFFFFFFFF;
	return ent;
}

static vfs_node_t *fat_entry2node(fat_entry_t *entry, fat_superblock_t *fat_superblock) {
	fat_inode_t *inode = kmalloc(sizeof(fat_inode_t));
	memset(inode, 0, sizeof(fat_inode_t));
	inode->entry    = *entry;
	inode->first_cluster = (entry->cluster_higher << 16) | entry->cluster_lower;

	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node, 0, sizeof(vfs_node_t));
	node->private_inode = inode;
	node->ops           = &fat_inode_ops;
	node->superblock    = &fat_superblock->superblock;
	node->ref_count = 1;
	if (entry->attribute & ATTR_DIRECTORY) {
		node->flags = VFS_DIR;
	} else {
		node->flags = VFS_FILE;
	}
	return node;
}

static ssize_t fat_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	fat_inode_t *inode = fd->private;
	fat_superblock_t *fat_superblock = container_of(fd->inode->superblock, fat_superblock_t, superblock);

	if (offset > inode->entry.file_size) {
		return 0;
	} else if (offset + count > inode->entry.file_size) {
		count = inode->entry.file_size - offset;
	}

	uint32_t start = offset / fat_superblock->cluster_size;
	uint32_t end   = (offset + count + fat_superblock->cluster_size - 1) / fat_superblock->cluster_size;
	uint32_t cluster = inode->first_cluster;

	//amount of data read
	size_t data_read = 0;

	//start by going to the first cluster
	for (size_t i=0; i < start; i++) {
		//early EOF ??? probably corrupted fat fs
		if (cluster == FAT_EOF)return -EIO;
		cluster = fat_get_next_cluster(fat_superblock, cluster);
	}
	for (size_t i=start; i < end; i++) {
		//early EOF ??? probably corrupted fat fs
		if (cluster == FAT_EOF)return -EIO;

		size_t off = fat_cluster2offset(fat_superblock, cluster);
		size_t read_size = min(count, fat_superblock->cluster_size);
		if (i == start) {
			//allow unaligned read
			off += offset % fat_superblock->cluster_size;

			if (read_size + (offset % fat_superblock->cluster_size) > fat_superblock->cluster_size) {
				read_size = fat_superblock->cluster_size - (offset % fat_superblock->cluster_size);
			}
		}


		ssize_t r = vfs_read(fat_superblock->superblock.device, buf, off, read_size);

		if (r < 0)return r;
		data_read += r;
		if ((size_t)r < min(count, fat_superblock->cluster_size))return data_read;
		buf = (char *)buf + r;
		count -= r;

		cluster = fat_get_next_cluster(fat_superblock, cluster);
	}

	return data_read;
}

/**
 * @brief parse fat entries and make a dirent from it
 */
static int fat2dirent(fat_superblock_t *fat_superblock, fat_inode_t *inode, size_t offset, struct dirent *dirent) {
	fat_entry_t entry;
	vfs_read(fat_superblock->superblock.device, &entry, offset, sizeof(entry));

	if (entry.name[0] == 0x00) {
		// everything is free after that
		// we hit last
		return -ENOENT;
	}

	if ((entry.attribute & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
		kdebugf("long name\n");
		// we a long name
		size_t j=0;
		size_t ord = 1;
		fat_long_entry_t long_entry;

		for (;;) {
			vfs_read(fat_superblock->superblock.device, &long_entry, offset, sizeof(entry));
			if ((long_entry.ord & ~LAST_LONG_ENTRY) != ord++) {
				// corrupted
				return -EIO;
			}
			if (!(long_entry.attribute & ATTR_LONG_NAME)) {
				// corrupted
				return -EIO;
			}

			// append
			for (size_t i=0; i < sizeof(long_entry.name1); i++) {
				if (!long_entry.name1[i]) goto cont;
				dirent->d_name[j++] = long_entry.name1[i];
			}
			for (size_t i=0; i < sizeof(long_entry.name2); i++) {
				if (!long_entry.name2[i]) goto cont;
				dirent->d_name[j++] = long_entry.name2[i];
			}
			for (size_t i=0; i < sizeof(long_entry.name3); i++) {
				if (!long_entry.name3[i]) goto cont;
				dirent->d_name[j++] = long_entry.name3[i];
			}

			// next entry
			cont:
			offset += sizeof(fat_entry_t);
			if (!inode->is_fat16_root && !(offset % fat_superblock->cluster_size)) {
				// end of cluster
				// jump to next cluster
				uint32_t cluster = fat_get_next_cluster(fat_superblock, cluster);
				if (cluster == FAT_EOF)return -ENOENT;
				offset = fat_cluster2offset(fat_superblock, cluster);
			}

			if (long_entry.ord & LAST_LONG_ENTRY) break;
		}
		vfs_read(fat_superblock->superblock.device, &entry, offset, sizeof(entry));
		dirent->d_name[j] = '\0';
	} else {
		size_t j=0;
		for (int i=0; i < 8; i++) {
			if (entry.name[i] == ' ')break;
			dirent->d_name[j++] = entry.name[i];
		}
		//don't add . for directories without extention
		if (!((entry.attribute & ATTR_DIRECTORY) && entry.name[8] == ' ')) {
			dirent->d_name[j++] = '.';
		}
		for (int i=8; i < 11; i++) {
			if (entry.name[i] == ' ')break;
			dirent->d_name[j++] = entry.name[i];
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

static int fat_readdir(vfs_node_t *node, unsigned long index, struct dirent *dirent) {
	fat_inode_t *inode = node->private_inode;
	fat_superblock_t *fat_superblock = container_of(node->superblock, fat_superblock_t, superblock);

	size_t offset   = inode->is_fat16_root ? inode->start : fat_cluster2offset(fat_superblock, inode->first_cluster);
	uint32_t remaning = inode->is_fat16_root ? inode->entries_count : UINT32_MAX;
	uint32_t cluster  = inode->first_cluster;
	kdebugf("readdir on %s , first cluster is %lx\n", inode->is_fat16_root ? "root" : "not root", cluster);
	while (index > 0) {
		if (!remaning) return -ENOENT;
		// skip everything with VOLUME_ID attr or free
		for (;;) {
			fat_entry_t entry;
			vfs_read(fat_superblock->superblock.device, &entry, offset, sizeof(entry));
			if (entry.name[0] == 0x00) {
				// everything is free after that
				// we hit last
				return -ENOENT;
			}
			if (!(entry.attribute & ATTR_VOLUME_ID) && (entry.name[0] != (char)0xe5))break;
			offset += sizeof(fat_entry_t);
			if (!inode->is_fat16_root && !(offset % fat_superblock->cluster_size)) {
				//end of cluster
				//jump to next
				cluster = fat_get_next_cluster(fat_superblock, cluster);
				if (cluster == FAT_EOF)return -ENOENT;
				offset = fat_cluster2offset(fat_superblock, cluster);
			}
			remaning--;
			if (!remaning)return -ENOENT;
		}
		index--;
		offset += sizeof(fat_entry_t);
		if (!inode->is_fat16_root && !(offset % fat_superblock->cluster_size)) {
			//end of cluster
			//jump to next
			cluster = fat_get_next_cluster(fat_superblock, cluster);
			if (cluster == FAT_EOF) return -ENOENT;
			offset = fat_cluster2offset(fat_superblock, cluster);
		}
		remaning--;
	}
	if (!remaning) return -ENOENT;

	return fat2dirent(fat_superblock, inode, offset, dirent);
}

static int fat_lookup(vfs_node_t *node, vfs_dentry_t *dentry, const char *name) {
	fat_inode_t *inode = node->private_inode;
	fat_superblock_t *fat_superblock = container_of(node->superblock, fat_superblock_t, superblock);

	uint32_t remaning = inode->is_fat16_root ? inode->entries_count : UINT32_MAX;
	uint64_t offset   = inode->is_fat16_root ? inode->start : fat_cluster2offset(fat_superblock, inode->first_cluster);
	uint32_t cluster  = inode->first_cluster;

	while (remaning > 0) {
		fat_entry_t entry;
		vfs_read(fat_superblock->superblock.device, &entry, offset, sizeof(entry));
		offset += sizeof(fat_entry_t);
		if (!inode->is_fat16_root && !(offset % fat_superblock->cluster_size)) {
			//end of cluster
			//jump to next
			cluster = fat_get_next_cluster(fat_superblock, cluster);
			if (cluster == FAT_EOF)return -ENOENT;
			offset = fat_cluster2offset(fat_superblock, cluster);
		}
		if (inode->is_fat16_root)remaning--;

		if (entry.name[0] == 0x00) {
			//everything is free after that
			//we hit last
			return -ENOENT;
		}
		//TODO : long name support
		if ((entry.attribute & ATTR_VOLUME_ID) || (entry.name[0] == (char)0xe5))cont: continue;

		size_t j = 0;
		for (int i=0; i < 8; i++) {
			if (entry.name[i] == ' ')break;
			//broken entry check
			if (entry.name[i] < 0x20)goto cont;
			if (name[j++] != entry.name[i])goto cont;
		}
		if (name[j] == '.')j++;
		for (int i=8; i < 11; i++) {
			if (entry.name[i] == ' ')break;
			//broken entry check
			if (entry.name[i] < 0x20)goto cont;
			if (name[j++] != entry.name[i])goto cont;
		}
		if (name[j])continue;

		//we found it
		dentry->inode = fat_entry2node(&entry, fat_superblock);
		// TODO : inode number
		return 0;
	}

	return -ENOENT;
}

static void fat_cleanup(vfs_node_t *node) {
	kfree(node->private_inode);
}

static int fat_open(vfs_fd_t *fd) {
	fd->ops = &fat_fd_ops;
	return 0;
}

int fat_mount(const char *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)flags;
	(void)data;
	(void)target;
	vfs_fd_t *dev = vfs_open(source, O_RDONLY);
	if (!dev)return -ENOENT;

	fat_bpb bpb;
	if (vfs_read(dev, &bpb, 0, sizeof(bpb)) != sizeof(bpb)) {
		vfs_close(dev);
		return -EIO; //not sure this is the good error code
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

	uint16_t root_sectors = ((bpb.root_entires_count * 32) + bpb.byte_per_sector - 1) / bpb.byte_per_sector;
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
		//TODO : fat12/16 check
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
		//build a fake entry for root
		fat_entry_t root_entry;
		memset(&root_entry, 0, sizeof(root_entry));
		root_entry.attribute = ATTR_DIRECTORY;
		root_entry.cluster_lower  =  bpb.extended.fat32.root_cluster & 0xf;
		root_entry.cluster_higher =  (bpb.extended.fat32.root_cluster >> 8) & 0xf;
		//how do we get size ?
		local_root = fat_entry2node(&root_entry, fat_superblock);
	} else {
		//root of fat12/16 is really stupid
		fat_inode_t *root = kmalloc(sizeof(fat_inode_t));
		root->entries_count = bpb.root_entires_count;
		root->start         = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector;
		root->is_fat16_root = 1;

		local_root = kmalloc(sizeof(vfs_node_t));
		memset(local_root, 0, sizeof(vfs_node_t));
		local_root->private_inode = root;
		local_root->flags = VFS_DIR;
		local_root->ops = &fat_inode_ops;
		local_root->superblock = &fat_superblock->superblock;
	}
	local_root->ref_count = 1;
	fat_superblock->superblock.root = local_root;
	*superblock_out = &fat_superblock->superblock;
	return 0;
}

static vfs_inode_ops_t fat_inode_ops = {
	.readdir = fat_readdir,
	.lookup  = fat_lookup,
	.getattr = fat_getattr,
	.cleanup = fat_cleanup,
	.open    = fat_open,
};

static vfs_fd_ops_t fat_fd_ops = {
	.read    = fat_read,
};

static vfs_filesystem_t fat_fs = {
	.mount = fat_mount,
	.name = "fat",
};

int fat_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	vfs_register_fs(&fat_fs);
	return 0;
}

int fat_fini() {
	//TODO : make sure no partition is mounted
	vfs_unregister_fs(&fat_fs);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = fat_init,
	.fini = fat_fini,
	.author = "tayoky",
	.name = "fat",
	.description = "fat12/16/32 drivers",
	.license = "GPL 3",
};
