#include <kernel/module.h>
#include <kernel/slab.h>
#include <kernel/vfs.h>
#include <module/retrofs.h>

static slab_cache_t retrofs_inodes_slab;

static retrofs_inode_t *retrofs_entry2inode(retrofs_superblock_t *superblock, retrofs_directory_entry_t *entry, off_t offset);

static vfs_inode_ops_t retrofs_inode_ops = {
	// TODO
};

static retrofs_inode_t *retrofs_entry2inode(retrofs_superblock_t *retrofs_superblock, retrofs_directory_entry_t *entry, off_t offset) {
	retrofs_inode_t *inode = slab_alloc(&retrofs_inodes_slab);
	if (!inode) return NULL;
	inode->vnode.superblock = &retrofs_superblock->superblock;
	inode->vnode.atime = entry->creation_time;
	inode->vnode.mtime = entry->modification_time;
	inode->vnode.ctime = entry->creation_time;
	inode->vnode.mode  = 0777;
	if (entry->flags & RETROFS_FLAG_DIRECTORY) {
		inode->vnode.mode |= S_IFDIR;
	} else {
		inode->vnode.mode |= S_IFREG;
		init_cache(&inode->cache);
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
	retrofs_superblock->superblock.device = vfs_dup(source);
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
