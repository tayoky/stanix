#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/time.h>
#include <kernel/vfs.h>
#include <module/iso9660.h>

static slab_cache_t iso9660_inodes_slab;

static iso9660_inode_t *iso9660_entry2inode(iso9660_dentry_t *dentry);

static time_t iso9660_convert_small_time(iso9660_small_time_t *iso9660_time) {
	time_t time = date2time(iso9660_time->year + 1900, 
			iso9660_time->month,
			iso9660_time->day,
			iso9660_time->hour,
			iso9660_time->minute,
			iso9660_time->second);
	// apply offset, because unix timestamps are in UTC
	time -= iso9660_time->timezone * 15 * 60;
	return time;
}

static void *iso9660_get_susp_entry_start(iso9660_dentry_t *dentry, const char *name, void *start) {
	char *ptr = start;
	char *end = (char*)dentry + dentry->length;
	while (ptr + sizeof(iso9660_susp_entry_t) <= end) {
		iso9660_susp_entry_t *entry = (iso9660_susp_entry_t*)ptr;
		if (!memcmp(entry->name, name, sizeof(entry->name))) {
			return entry;
		}
		ptr += entry->length;
	}
	return NULL;
}

static void *iso9660_get_susp_entry(iso9660_dentry_t *dentry, const char *name) {
	char *start = dentry->file_identifier + dentry->filename_length;
	if (dentry->filename_length % 2 == 0) {
		// skip the padding byte
		start++;
	}

	return iso9660_get_susp_entry_start(dentry, name, start);
}

static void *iso9660_get_susp_entry_after(iso9660_dentry_t *dentry, const char *name, void *data) {
	iso9660_susp_entry_t *entry = data;

	char *start = (char*)entry + entry->length;
	return iso9660_get_susp_entry_start(dentry, name, start);
}

static void iso9660_extract_name(iso9660_dentry_t *dentry, char buf[256]) {
	// TODO : rock ridger name support
	ssize_t name_length = 0;
	while (name_length < dentry->filename_length && name_length + (ssize_t)sizeof(iso9660_dentry_t) < dentry->length && dentry->file_identifier[name_length] != ';') {
		name_length++;
	}
	if (name_length >= 256) name_length = 255;
	memcpy(buf, dentry->file_identifier, name_length);
	buf[name_length] = '\0';

	if (name_length == 1) {
		// handling of special entries
		switch (dentry->file_identifier[0]) {
		case '\0':
			strcpy(buf, ".");
			break;
		case '\1':
			strcpy(buf, "..");
			break;
		}
	}
}

static int iso9660_read_pages(cache_t *cache, off_t offset, size_t count) {
	iso9660_inode_t *inode = container_of(cache, iso9660_inode_t, cache);
	iso9660_superblock_t *iso9660_superblock = container_of(inode->vnode.superblock, iso9660_superblock_t, superblock);

	// TODO : we need a way to pass the pages direcly
	off_t start_offset = inode->lba * iso9660_superblock->block_size;
	for (uintptr_t addr = offset; addr < offset + count; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(&inode->cache, addr);
		kassert(page != PAGE_INVALID);
		ssize_t ret = vfs_read(iso9660_superblock->superblock.device, mmu_phys2virt(page), start_offset + addr, PAGE_SIZE);
		if (ret < 0) return ret;
		if (ret < PAGE_SIZE) return -EIO;
	}
	return 0;
}

static cache_ops_t iso9660_cache_ops = {
	.read = iso9660_read_pages,
};

static ssize_t iso9660_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	iso9660_inode_t *inode = fd->private;
	return cache_read(&inode->cache, buffer, offset, count);
}

static ssize_t iso9660_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)fd;
	(void)buffer;
	(void)offset;
	(void)count;
	return -EROFS;
}

static vfs_fd_ops_t iso9660_fd_ops = {
	.read  = iso9660_read,
	.write = iso9660_write,
};

static int iso9660_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	iso9660_inode_t *inode = container_of(vnode, iso9660_inode_t, vnode);
	iso9660_superblock_t *iso9660_superblock = container_of(inode->vnode.superblock, iso9660_superblock_t, superblock);
	off_t offset = inode->lba * iso9660_superblock->block_size;
	off_t end = offset + inode->size;

	while (offset < end) {
		char buf[256];
		ssize_t ret = vfs_read(iso9660_superblock->superblock.device, buf, offset, sizeof(buf));
		if (ret < 0) return ret;
		if (ret < (ssize_t)sizeof(iso9660_dentry_t)) return -EIO;

		iso9660_dentry_t *dentry = (iso9660_dentry_t*)buf;
		if (dentry->length < (ssize_t)sizeof(iso9660_dentry_t)) return -EIO;
		if (ret < dentry->length) return -EIO;
		
		if (index-- > 0) {
			offset += dentry->length;
			continue;
		}

		iso9660_extract_name(dentry, dirent->d_name);
		return 0;
	}

	return -ENOENT;
}

static int iso9660_lookup(vfs_node_t *vnode, vfs_dentry_t *dentry) {
	return -ENOSYS;
}

static int iso9660_getattr(vfs_node_t *vnode, struct stat *buf) {
	iso9660_inode_t *inode = container_of(vnode, iso9660_inode_t, vnode);
	buf->st_nlink = inode->nlink;
	buf->st_size  = inode->size;
	return 0;
}

static int iso9660_rdonly() {
	return -EROFS;
}

static int iso9660_open(vfs_fd_t *fd) {
	iso9660_inode_t *inode = container_of(fd->inode, iso9660_inode_t, vnode);
	fd->private = inode;
	fd->ops = &iso9660_fd_ops;
	return 0;
}

static void iso9660_cleanup(vfs_node_t *vnode) {
	iso9660_inode_t *inode = container_of(vnode, iso9660_inode_t, vnode);
	if (S_ISREG(inode->vnode.mode)) {
		free_cache(&inode->cache);
	}

	slab_free(inode);
}

static vfs_inode_ops_t iso9660_inode_ops = {
	.readdir  = iso9660_readdir,
	.lookup   = iso9660_lookup,
	.getattr  = iso9660_getattr,
	.setattr  = iso9660_rdonly,
	.truncate = iso9660_rdonly,
	.create   = iso9660_rdonly,
	.mkdir    = iso9660_rdonly,
	.mknod    = iso9660_rdonly,
	.link     = iso9660_rdonly,
	.symlink  = iso9660_rdonly,
	.rename   = iso9660_rdonly,
	.unlink   = iso9660_rdonly,
	.rmdir	  = iso9660_rdonly,
	.open     = iso9660_open,
	.cleanup  = iso9660_cleanup,
};

static iso9660_inode_t *iso9660_entry2inode(iso9660_dentry_t *dentry) {
	iso9660_inode_t *inode = slab_alloc(&iso9660_inodes_slab);
	if (!inode) return NULL;
	inode->vnode.ops        = &iso9660_inode_ops;
	inode->vnode.ref_count  = 1;
	time_t time = iso9660_convert_small_time(&dentry->time);
	inode->vnode.atime = time;
	inode->vnode.mtime = time;
	inode->vnode.ctime = time;
	inode->vnode.mode  = 0777;
	inode->lba         = le_uint32_to_uint32(&dentry->lba.le);
	inode->size        = le_uint32_to_uint32(&dentry->data_length.le);
	inode->nlink       = 1;

	iso9660_px_entry_t *px = iso9660_get_susp_entry(dentry, ISO9660_PX_ENTRY);
	if (px && px->susp_entry.length != sizeof(iso9660_px_entry_t)) px = NULL;
	if (px && px->version != ISO9660_PX_ENTRY_VERSION) px = NULL;

	iso9660_pn_entry_t *pn = iso9660_get_susp_entry(dentry, ISO9660_PN_ENTRY);
	if (pn && pn->susp_entry.length != sizeof(iso9660_pn_entry_t)) pn = NULL;
	if (pn && pn->version != ISO9660_PN_ENTRY_VERSION) pn = NULL;

	if (px) {
		// TODO : use inode cache maybee
		inode->vnode.mode   = le_uint32_to_uint32(&px->mode.le);
		inode->vnode.uid    = le_uint32_to_uint32(&px->uid.le);
		inode->vnode.gid    = le_uint32_to_uint32(&px->gid.le);
		inode->vnode.number = le_uint32_to_uint32(&px->inode.le);
		inode->nlink  = le_uint32_to_uint32(&px->nlink.le);
		if (pn) {
			inode->dev = ((uint64_t)le_uint32_to_uint32(&pn->dev_high.le) << 32) | le_uint32_to_uint32(&pn->dev_low.le);
		}
	} else if (dentry->flags & ISO9660_DENTRY_FLAG_DIRECTORY) {
		inode->vnode.mode |= S_IFDIR;
	} else {
		inode->vnode.mode |= S_IFREG;
		init_cache(&inode->cache);
		inode->cache.ops  = &iso9660_cache_ops;
		inode->cache.size = inode->size;
	}
	return inode;
}

static int iso9660_mount(vfs_fd_t *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)flags;
	(void)data;
	(void)target;

	size_t block_size = 0;
	iso9660_inode_t *root = NULL;

	// iterate through each volume descriptor
	iso9660_volume_descriptor_t volume_descriptor = {0};
	for (size_t offset = 32 * 1024; volume_descriptor.type != ISO9660_VOLUME_DESCRIPTOR_SET_TERMINATOR; offset += sizeof(iso9660_volume_descriptor_t)) {
		ssize_t ret = vfs_read(source, &volume_descriptor, sizeof(volume_descriptor), offset);
		if (ret < 0) return ret;
		if (ret < (ssize_t)sizeof(volume_descriptor)) return -EIO;

		if (volume_descriptor.type != ISO9660_VOLUME_DESCRIPTOR_PRIMARY) {
			// not a primary descriptor, we don't care
			continue;
		}

		// check if the version is valid
		if (volume_descriptor.version != 0x01) {
			kwarningf("unsupported version %hhx\n", volume_descriptor.version);
			return -ENOTSUP;
		}

		if (root) {
			kwarningf("two primary volume descriptors, ignoring second\n");
			continue;
		}

		// TODO : handle stuff
		// extract root dir, setup superblock with block siee, ...
		block_size = le_uint16_to_uint16(&volume_descriptor.primary.logical_block_size.le);
		
		iso9660_dentry_t *root_dentry = (iso9660_dentry_t*)volume_descriptor.primary.root_dentry;
		if (root_dentry->length != sizeof(volume_descriptor.primary.root_dentry)) {
			kwarningf("invalid root dentry length\n");
			return -EFTYPE;
		}

		root = iso9660_entry2inode(root_dentry);
		if (!root) return -ENOMEM;
	}

	if (!root) {
		kwarningf("no primary descriptor found\n");
		return -EFTYPE;
	}

	iso9660_superblock_t *iso9660_superblock = kmalloc(sizeof(iso9660_superblock_t));
	if (!iso9660_superblock) {
		vfs_node_release(&root->vnode);
		return -ENOMEM;
	}
	memset(iso9660_superblock, 9, sizeof(iso9660_superblock_t));
	iso9660_superblock->superblock.device = source;
	iso9660_superblock->superblock.root   = &root->vnode;
	iso9660_superblock->block_size = block_size;
	*superblock_out = &iso9660_superblock->superblock;
	return 0;
}

static vfs_filesystem_t iso9660_fs = {
	.mount = iso9660_mount,
	.name  = "iso9660",
};

static int iso9660_inode_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	iso9660_inode_t *inode = data;
	memset(inode, 0, sizeof(iso9660_inode_t));
	return 0;
}

int iso9660_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&iso9660_inodes_slab, sizeof(iso9660_inode_t), "iso9660-inodes");
	iso9660_inodes_slab.constructor = iso9660_inode_constructor;
	vfs_register_fs(&iso9660_fs);
	return 0;
}

int iso9660_fini(void) {
	int ret = vfs_unregister_fs(&iso9660_fs);
	if (ret < 0) return ret;
	slab_destroy(&iso9660_inodes_slab);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = iso9660_init,
	.fini        = iso9660_fini,
	.author      = "tayoky",
	.name        = "iso9660",
	.description = "iso9660 filesystem driver",
	.license     = "GPL 3",
};
