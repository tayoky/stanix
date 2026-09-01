#include <kernel/userspace.h>
#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/time.h>
#include <kernel/vfs.h>
#include <module/iso9660.h>

static slab_cache_t iso9660_inodes_slab;

static iso9660_inode_t *iso9660_entry2inode(iso9660_superblock_t *iso9660_superblock, iso9660_dentry_t *dentry);

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
		if (entry->length < sizeof(iso9660_susp_entry_t)) {
			// invalid entry
			break;
		}
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

static void *iso9660_get_next_susp_entry(iso9660_dentry_t *dentry, const char *name, void *data) {
	iso9660_susp_entry_t *entry = data;

	char *start = (char*)entry + entry->length;
	return iso9660_get_susp_entry_start(dentry, name, start);
}

static void iso9660_extract_name(iso9660_dentry_t *dentry, char *buf, size_t buf_size) {
	iso9660_nm_entry_t *nm = iso9660_get_susp_entry(dentry, ISO9660_NM_ENTRY);
	if (nm && nm->susp_entry.length < sizeof(iso9660_nm_entry_t)) nm = NULL;
	if (nm && nm->version != ISO9660_NM_ENTRY_VERSION) nm = NULL;
	if (nm) {
		// we have a rock ridger name
		size_t ptr = 0;
		for (;;) {
			if (nm->flags & ISO9660_NM_ENTRY_FLAG_CURRENT) {
				if (ptr + 1 < buf_size) buf[ptr++] = '.';
			} else if (nm->flags & ISO9660_NM_ENTRY_FLAG_PARENT) {
				if (ptr + 1 < buf_size) buf[ptr++] = '.';
				if (ptr + 1 < buf_size) buf[ptr++] = '.';
			} else {
				size_t data_length = nm->susp_entry.length - sizeof(iso9660_nm_entry_t);
				if (data_length >= buf_size - ptr) data_length = buf_size - ptr - 1;
				memcpy(&buf[ptr], nm->data, data_length);
				ptr += data_length;
			}

			if (nm->flags & ISO9660_NM_ENTRY_FLAG_CONTINUE) {
				nm = iso9660_get_next_susp_entry(dentry, ISO9660_NM_ENTRY, nm);
				if (nm && nm->susp_entry.length < sizeof(iso9660_nm_entry_t)) nm = NULL;
				if (nm && nm->version != ISO9660_NM_ENTRY_VERSION) nm = NULL;
				if (!nm) {
					// corrupt long name??
					// fallback to short name so userspace has at least something to see
					goto short_name;
				}
			} else {
				// this was the last entry
				break;
			}
		}
		buf[ptr] = '\0';
		return;
	}

short_name:
	ssize_t name_length = 0;
	while (name_length < dentry->filename_length && name_length + (ssize_t)sizeof(iso9660_dentry_t) < dentry->length && dentry->file_identifier[name_length] != ';') {
		name_length++;
	}
	if (name_length >= (ssize_t)buf_size) name_length = buf_size - 1;
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

static int iso9660_extract_symlink(iso9660_dentry_t *dentry, char *buf, size_t buf_size) {
	iso9660_sl_entry_t *sl = iso9660_get_susp_entry(dentry, ISO9660_SL_ENTRY);
	if (!sl) return -ENOENT;
	if (sl->susp_entry.length < sizeof(iso9660_sl_entry_t)) return -ENOENT;
	if (sl->version != ISO9660_SL_ENTRY_VERSION) return -ENOENT;

	int skip_slash = 1;
	size_t ptr = 0;
	for (;;) {
		for (size_t offset = 0; offset < sl->susp_entry.length - sizeof(iso9660_sl_entry_t);) {
			iso9660_sl_component_t *component = (iso9660_sl_component_t*)(sl->components + offset);
			if (component->length < sizeof(iso9660_sl_component_t)) {
				// invalid component entry
				return -EFTYPE;
			}

			if (skip_slash) {
				skip_slash = 0;
			} else {
				if (ptr + 1 >= buf_size) return -ERANGE;
				buf[ptr++] = '/';
			}

			if (component->flags & ISO9660_SL_COMPONENT_FLAG_CURRENT) {
				if (ptr + 1 >= buf_size) return -ERANGE;
				buf[ptr++] = '.';
			} else  if (component->flags & ISO9660_SL_COMPONENT_FLAG_PARENT) {
				if (ptr + 2 >= buf_size) return -ERANGE;
				buf[ptr++] = '.';
				buf[ptr++] = '.';
			} else if (component->flags & ISO9660_SL_COMPONENT_FLAG_ROOT) {
				if (ptr + 1 >= buf_size) return -ERANGE;
				buf[ptr++] = '/';

				// avoid unecessary double slash
				skip_slash = 0;
			} else {
				// raw data component
				size_t data_length = component->length - sizeof(iso9660_sl_component_t);
				if (ptr + data_length >= buf_size) return -ERANGE;
				memcpy(&buf[ptr], component->data, data_length);
				ptr += data_length;
			}
			if (component->flags & ISO9660_SL_COMPONENT_FLAG_CONTINUE) {
				skip_slash = 1;
			}
			offset += component->length;
		}
		if (sl->flags & ISO9660_SL_ENTRY_FLAG_CONTINUE) {
			sl = iso9660_get_next_susp_entry(dentry, ISO9660_SL_ENTRY, sl);
			if (sl->susp_entry.length < sizeof(iso9660_sl_entry_t)) return -ENOENT;
			if (sl->version != ISO9660_SL_ENTRY_VERSION) return -ENOENT;
			if (!sl) {
				// not good, the sl list did not terminate correcly
				return -EFTYPE;
			}
			continue;
		}
		break;
	}

	kassert(ptr < buf_size);
	buf[ptr] = '\0';
	return 0;
}

static int iso9660_read_dentry(iso9660_superblock_t *iso9660_superblock, char *buf, size_t buf_size, off_t offset) {
	ssize_t ret = vfs_read(iso9660_superblock->superblock.device, buf, offset, buf_size);
	if (ret < 0) return ret;
	if (ret < (ssize_t)sizeof(iso9660_dentry_t)) return -EIO;

	iso9660_dentry_t *dentry = (iso9660_dentry_t*)buf;
	if (dentry->length < (ssize_t)sizeof(iso9660_dentry_t)) return -EIO;
	if (ret < dentry->length) return -EIO;
	return 0;
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
	cache_read_terminate(cache, offset, count, 0);
	return 0;
}

static cache_ops_t iso9660_cache_ops = {
	.read = iso9660_read_pages,
};

static ssize_t iso9660_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	iso9660_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	return cache_read(&inode->cache, buffer, offset, count);
}

static ssize_t iso9660_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)fd;
	(void)buffer;
	(void)offset;
	(void)count;
	return -EROFS;
}

static int iso9660_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	iso9660_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	return cache_mmap(&inode->cache, offset, seg);
}

static vfs_fd_ops_t iso9660_fd_ops = {
	.read  = iso9660_read,
	.write = iso9660_write,
	.mmap  = iso9660_mmap,
};

static int iso9660_readdir(vfs_node_t *vnode, unsigned long index, struct dirent *dirent) {
	iso9660_inode_t *inode = container_of(vnode, iso9660_inode_t, vnode);
	kassert(S_ISDIR(inode->vnode.mode));
	iso9660_superblock_t *iso9660_superblock = container_of(inode->vnode.superblock, iso9660_superblock_t, superblock);
	off_t offset = inode->lba * iso9660_superblock->block_size;
	off_t end = offset + inode->size;

	while (offset < end) {
		char buf[256];
		int ret = iso9660_read_dentry(iso9660_superblock, buf, sizeof(buf), offset);
		if (ret < 0) return ret;
		iso9660_dentry_t *dentry = (iso9660_dentry_t*)buf;
		
		if (index-- == 0) {
			iso9660_extract_name(dentry, dirent->d_name, sizeof(dirent->d_name));

			iso9660_px_entry_t *px = iso9660_get_susp_entry(dentry, ISO9660_PX_ENTRY);
			if (px && px->susp_entry.length != sizeof(iso9660_px_entry_t)) px = NULL;
			if (px && px->version != ISO9660_PX_ENTRY_VERSION) px = NULL;

			if (px) {
				switch (le_uint32_to_uint32(&px->mode.le) & S_IFMT) {
				case S_IFREG:
					dirent->d_type = DT_REG;
					break;
				case S_IFDIR:
					dirent->d_type = DT_DIR;
					break;
				case S_IFIFO:
					dirent->d_type = DT_FIFO;
					break;
				case S_IFSOCK:
					dirent->d_type = DT_SOCK;
					break;
				case S_IFCHR:
					dirent->d_type = DT_CHR;
					break;
				case S_IFBLK:
					dirent->d_type = DT_BLK;
					break;
				case S_IFLNK:
					dirent->d_type = DT_LNK;
					break;
				}
			} else if (dentry->flags & ISO9660_DENTRY_FLAG_DIRECTORY) {
				dirent->d_type = DT_DIR;
			} else {
				dirent->d_type = DT_REG;
			}
			return 0;
		}
		offset += dentry->length;
	}

	return -ENOENT;
}

static int iso9660_lookup(vfs_node_t *vnode, vfs_dentry_t *vfs_dentry) {
	iso9660_inode_t *inode = container_of(vnode, iso9660_inode_t, vnode);
	kassert(S_ISDIR(inode->vnode.mode));
	iso9660_superblock_t *iso9660_superblock = container_of(inode->vnode.superblock, iso9660_superblock_t, superblock);
	off_t offset = inode->lba * iso9660_superblock->block_size;
	off_t end = offset + inode->size;

	while (offset < end) {
		char buf[256];
		int ret = iso9660_read_dentry(iso9660_superblock, buf, sizeof(buf), offset);
		if (ret < 0) return ret;
		iso9660_dentry_t *dentry = (iso9660_dentry_t*)buf;

		char name[256];
		iso9660_extract_name(dentry, name, sizeof(name));

		if (!strcmp(name, vfs_dentry->name)) {
			iso9660_inode_t *inode = iso9660_entry2inode(iso9660_superblock, dentry);
			if (!inode) return -ENOMEM;
			vfs_dentry->inode = &inode->vnode;
			return 0;
		}
		offset += dentry->length;
	}
	return -ENOENT;
}

static ssize_t iso9660_readlink(vfs_node_t *vnode, char *buf, size_t bufsize) {
	iso9660_inode_t *inode = container_of(vnode, iso9660_inode_t, vnode);
	if (!inode->link) return -EIO;
	if (bufsize > strlen(inode->link)) bufsize = strlen(inode->link);
	int ret = safe_copy_to(buf, inode->link, bufsize);
	return ret < 0 ? ret : (ssize_t)bufsize;
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
	} else if (S_ISLNK(inode->vnode.mode)) {
		kfree(inode->link);
	}

	slab_free(inode);
}

static vfs_inode_ops_t iso9660_inode_ops = {
	.readdir  = iso9660_readdir,
	.lookup   = iso9660_lookup,
	.readlink = iso9660_readlink,
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

static iso9660_inode_t *iso9660_entry2inode(iso9660_superblock_t *iso9660_superblock, iso9660_dentry_t *dentry) {
	iso9660_inode_t *inode = slab_alloc(&iso9660_inodes_slab);
	if (!inode) return NULL;

	inode->vnode.superblock = &iso9660_superblock->superblock;
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
		kdebugf("got px\n");
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
	}

	if (S_ISREG(inode->vnode.mode)) {
		init_cache(&inode->cache);
		inode->cache.ops  = &iso9660_cache_ops;
		inode->cache.size = inode->size;
	} else if (S_ISLNK(inode->vnode.mode)) {
		char buf[256];
		if (iso9660_extract_symlink(dentry, buf, sizeof(buf)) >= 0) {
			inode->link = strdup(buf);
		}
	}
	return inode;
}

static int iso9660_probe(vfs_fd_t *source) {
	iso9660_volume_descriptor_t volume_descriptor;
	ssize_t ret = vfs_read(source, &volume_descriptor, 16 * 2048, sizeof(volume_descriptor));
	if (ret < (ssize_t)sizeof(iso9660_volume_descriptor_t)) return 0;
	if (memcmp(volume_descriptor.identifier, ISO9660_VOLUME_DESCRIPTOR_IDENTIFIER, sizeof(volume_descriptor.identifier))) return 0;
	if (volume_descriptor.version != ISO9660_VOLUME_DESCRIPTOR_VERSION) return 0;

	// use this over FAT
	return 2;
}

static int iso9660_mount(vfs_fd_t *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)flags;
	(void)data;
	(void)target;

	size_t block_size = 0;
	iso9660_inode_t *root = NULL;

	iso9660_superblock_t *iso9660_superblock = kmalloc(sizeof(iso9660_superblock_t));
	if (!iso9660_superblock) return -ENOMEM;
	memset(iso9660_superblock, 0, sizeof(iso9660_superblock_t));

	// iterate through each volume descriptor
	iso9660_volume_descriptor_t volume_descriptor = {0};
	for (size_t offset = 16 * 2048; volume_descriptor.type != ISO9660_VOLUME_DESCRIPTOR_SET_TERMINATOR; offset += sizeof(iso9660_volume_descriptor_t)) {
		if (offset >= 16 * 2048 + 65535 * sizeof(iso9660_volume_descriptor_t)) {
			kwarningf("no null terminator\n");
			return -EFTYPE;
		}
		ssize_t ret = vfs_read(source, &volume_descriptor, offset, sizeof(volume_descriptor));
		if (ret < 0) {
error:
			kfree(iso9660_superblock);
			slab_free(root);
			return ret;
		}
		if (ret < (ssize_t)sizeof(volume_descriptor)) {
			ret = -EIO;
			goto error;
		}

		if (memcmp(&volume_descriptor.identifier, ISO9660_VOLUME_DESCRIPTOR_IDENTIFIER, sizeof(volume_descriptor.identifier))) {
			kwarningf("invalid indentifier\n");
			ret = -EFTYPE;
			goto error;
		}

		if (volume_descriptor.type != ISO9660_VOLUME_DESCRIPTOR_PRIMARY) {
			// not a primary descriptor, we don't care
			continue;
		}

		// check if the version is valid
		if (volume_descriptor.version != ISO9660_VOLUME_DESCRIPTOR_VERSION) {
			kwarningf("unsupported version %hhx\n", volume_descriptor.version);
			ret = -ENOTSUP;
			goto error;
		}

		if (root) {
			kwarningf("two primary volume descriptors, ignoring second\n");
			continue;
		}

		// setup the superblock
		block_size = le_uint16_to_uint16(&volume_descriptor.primary.logical_block_size.le);
		
		iso9660_dentry_t *root_dentry = (iso9660_dentry_t*)volume_descriptor.primary.root_dentry;
		if (root_dentry->length != sizeof(volume_descriptor.primary.root_dentry)) {
			kwarningf("invalid root dentry length\n");
			ret = -EFTYPE;
			goto error;
		}

		root = iso9660_entry2inode(iso9660_superblock, root_dentry);
		if (!root) {
			ret = -ENOMEM;
			goto error;
		}
	}

	if (!root) {
		kwarningf("no primary descriptor found\n");
		kfree(iso9660_superblock);
		return -EFTYPE;
	}

	kdebugf("block size is %zu\n", block_size);
	iso9660_superblock->superblock.device = vfs_dup(source);
	iso9660_superblock->superblock.root   = &root->vnode;
	iso9660_superblock->superblock.ref_count = 1;
	iso9660_superblock->block_size = block_size;
	*superblock_out = &iso9660_superblock->superblock;
	return 0;
}

static vfs_filesystem_t iso9660_fs = {
	.probe = iso9660_probe,
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
