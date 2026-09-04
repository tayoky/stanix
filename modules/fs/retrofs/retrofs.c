#include <kernel/module.h>
#include <kernel/slab.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>
#include <module/retrofs.h>

static slab_cache_t retrofs_inodes_slab;

static retrofs_inode_t *retrofs_entry2inode(retrofs_superblock_t *superblock, retrofs_directory_entry_t *entry, off_t offset);

static int retrofs_read_pages(cache_t *cache, off_t offset, size_t count) {
	retrofs_inode_t *inode = container_of(cache, retrofs_inode_t, cache);
	retrofs_superblock_t *retrofs_superblock = container_of(inode->vnode.superblock, retrofs_superblock_t, superblock);

	// TODO : we need a way to pass the pages direcly
	off_t start_offset = inode->start_sector * RETROFS_SECTOE_SIZE;
	for (uintptr_t addr = offset; addr < offset + count; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(&inode->cache, addr);
		kassert(page != PAGE_INVALID);

		ssize_t remaining = inode->cache.size;
		ssize_t size = remaining < PAGE_SIZE ? remaining : PAGE_SIZE;
		ssize_t ret = vfs_read(retrofs_superblock->superblock.device, mmu_phys2virt(page), start_offset + addr, size);
		if (ret < 0) return ret;
		if (ret < size) return -EIO;
	}
	cache_read_terminate(cache, offset, count, 0);
	return 0;
}

static int retrofs_write_pages(cache_t *cache, off_t offset, size_t count) {
	retrofs_inode_t *inode = container_of(cache, retrofs_inode_t, cache);
	retrofs_superblock_t *retrofs_superblock = container_of(inode->vnode.superblock, retrofs_superblock_t, superblock);

	// TODO : we need a way to pass the pages direcly
	off_t start_offset = inode->start_sector * RETROFS_SECTOE_SIZE;
	for (uintptr_t addr = offset; addr < offset + count; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(&inode->cache, addr);
		kassert(page != PAGE_INVALID);

		ssize_t remaining = inode->cache.size;
		ssize_t size = remaining < PAGE_SIZE ? remaining : PAGE_SIZE;
		ssize_t ret = vfs_write(retrofs_superblock->superblock.device, mmu_phys2virt(page), start_offset + addr, size);
		if (ret < 0) return ret;
		if (ret < size) return -EIO;
	}
	cache_read_terminate(cache, offset, count, 0);
	return 0;
}

static cache_ops_t retrofs_cache_ops = {
	.read  = retrofs_read_pages,
	.write = retrofs_write_pages,
};

static ssize_t retrofs_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	retrofs_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	return cache_read(&inode->cache, buffer, offset, count);
}

static ssize_t retrofs_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	retrofs_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	return cache_write(&inode->cache, buffer, offset, count);
}

static int retrofs_flush(vfs_fd_t *fd, off_t offset, size_t count) {
	retrofs_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	int ret = cache_flush(&inode->cache, offset, count);
	if (ret < 0) return ret;
	// TODO : maybe do not sync the whole disk
	return vfs_flush(fd->inode->superblock->device);
}

static int retrofs_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	retrofs_inode_t *inode = fd->private;
	kassert(S_ISREG(inode->vnode.mode));
	return cache_mmap(&inode->cache, offset, seg);
}

static vfs_fd_ops_t retrofs_fd_ops = {
	.read  = retrofs_read,
	.write = retrofs_write,
	.flush = retrofs_flush,
	.mmap  = retrofs_mmap,
};

static int retrofs_open(vfs_fd_t *fd) {
	retrofs_inode_t *inode = container_of(fd->inode, retrofs_inode_t, inode);
	fd->private = inode;
	fd->ops = &retrofs_fd_ops;
	return 0;
}

static vfs_inode_ops_t retrofs_inode_ops = {
	.open = retrofs_open,
};

static retrofs_inode_t *retrofs_entry2inode(retrofs_superblock_t *retrofs_superblock, retrofs_directory_entry_t *entry, off_t offset) {
	retrofs_inode_t *inode = slab_alloc(&retrofs_inodes_slab);
	if (!inode) return NULL;
	inode->vnode.superblock = &retrofs_superblock->superblock;
	inode->vnode.atime  = entry->creation_time;
	inode->vnode.mtime  = entry->modification_time;
	inode->vnode.ctime  = entry->creation_time;
	inode->vnode.mode   = 0777;
	inode->entry_offset = offset;
	if (entry->flags & RETROFS_FLAG_DIRECTORY) {
		inode->vnode.mode |= S_IFDIR;
	} else {
		inode->vnode.mode |= S_IFREG;
		init_cache(&inode->cache);
		inode->cache.size = entry->length;
		inode->cache.ops = &retrofs_cache_ops;
	}
	return inode;
}

static int retrofs_probe(vfs_fd_t *source) {
	retrofs_descrition_block_t description_block;
	ssize_t ret = vfs_read(source, &description_block, 0, sizeof(description_block));
	if (ret < (ssize_t)sizeof(retrofs_description_block_t)) return 0;
	if (memcmp(description_block.identifier, RETROFS_IDENTIFIER, sizeof(description_block.identifier))) return 0;

	return 1;
}

static int retrofs_mount(vfs_fd_t *source, const char *target, unsigned long flags, const void *data, vfs_superblock_t **superblock_out) {
	(void)flags;
	(void)data;
	(void)target;

	retrofs_descrition_block_t description_block;
	ssize_t ret = vfs_read(source, &description_block, 0, sizeof(description_block));
	if (ret < 0) return ret;
	if (ret < (ssize_t)sizeof(retrofs_description_block_t)) return -EFTYPE;
	if (memcmp(description_block.identifier, RETROFS_IDENTIFIER, sizeof(description_block.identifier))) return -EFTYPE;

	retrofs_superblock_t *retrofs_superblock = kmalloc(sizeof(retrofs_superblock_t));
	if (!retrofs_superblock) return -ENOMEM;
	memset(retrofs_superblock, 0, sizeof(retrofs_superblock_t));
	
	retrofs_inode_t *root = slab_alloc(&retrofs_inodes_slab);
	if (!root) {
		kfree(retrofs_superblock);
		return -ENOMEM;
	}
	root->vnode.mode = S_IFDIR | 0777;
	root->vnode.ops = &retrofs_inode_ops;
	root->vnode.atime  = description_block->creation_time;
	root->vnode.mtime  = description_block->creation_time;
	root->vnode.ctime  = description_block->creation_time;
	root->start_sector = description_block->root_directory;

	retrofs_superblock->superblock.device = vfs_dup(source);
	retrofs_superblock->superblock.root   = &root->vnode;
	*superblock_out = &retrofs_superblock->superblock;

	return 0;
}

static vfs_filesystem_t retrofs_fs = {
	.name  = "retrofs",
	.probe = retrofs_probe,
	.mount = retrofs_mount,
};

static int retrofs_inode_constructor(slab_cache_t *cache, void *data) {
	(void)cache;
	retrofs_inode_t *inode = data;
	memset(inode, 0, sizeof(retrofs_inode_t));
	return 0;
}

int retrofs_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&retrofs_inodes_slab, sizeof(retrofs_inode_t), "retrofs-inodes");
	retrofs_inodes_slab.constructor = retrofs_inode_constructor;
	vfs_register_fs(&retrofs_fs);
	return 0;
}

int retrofs_fini(void) {
	int ret = vfs_unregister_fs(&retrofs_fs);
	if (ret < 0) return ret;
	slab_destroy(&retrofs_inodes_slab);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = retrofs_init,
	.fini        = retrofs_fini,
	.author      = "tayoky",
	.name        = "retrofs",
	.description = "retrofs filesystem driver",
	.license     = "GPL 3",
};
