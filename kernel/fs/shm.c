#include <kernel/vfs.h>
#include <kernel/list.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/page.h>
#include <kernel/memseg.h>
#include <kernel/shm.h>
#include <kernel/pmm.h>
#include <sys/mman.h>
#include <errno.h>

//TODO : make this safe using mutex

static list_t shm_files;
static vfs_node_t *shmfs_root;

//shared memory subsystem
int shmfs_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)source;
	(void)flags;
	(void)data;
	return vfs_mount(target,shmfs_root);
}

vfs_filesystem_t shm_fs = {
	.name = "shmfs",
	.mount = shmfs_mount,
};

static void shm_destroy(shm_file_t *file){
	foreach(node, &file->blocks){
		shm_block_t *block = (shm_block_t*)node;
		pmm_free_page(block->block);
		kfree(block);
	}
	kfree(file);
}

static int shm_mmap(vfs_fd_t *fd, off_t offset,memseg_t *seg){
	// why would you map shm as private ????
	if(!(seg->flags & MAP_SHARED)){
		return -EINVAL;
	}

	shm_file_t *file = fd->private;

	offset = PAGE_ALIGN_DOWN(offset);
	if(offset + seg->size > PAGE_ALIGN_UP(file->size))return -EINVAL;

	off_t off = 0;
	uintptr_t vaddr = seg->addr;
	foreach(node, &file->blocks){
		shm_block_t *block = (shm_block_t*)node;
		if(off >= offset && (size_t)off <= offset + seg->size){
				if(vaddr >= seg->addr + seg->size)break;
				map_page(get_addr_space(),block->block,vaddr,seg->prot);
				vaddr += PAGE_SIZE;
			}
			off += PAGE_SIZE;
		}

	return 0;
}

//op for the actual shm files   
static int shm_getattr(vfs_node_t *node,struct stat *st){
	shm_file_t *file = node->private_inode;
	st->st_uid  = file->uid;
	st->st_gid  = file->gid;
	st->st_mode = file->mode;
	st->st_size = file->size;
	st->st_blksize = PAGE_SIZE;
	st->st_blocks = PAGE_ALIGN_UP(st->st_size);
	return 0;
}

static int shm_setattr(vfs_node_t *node,struct stat *st){
	shm_file_t *file = node->private_inode;
	file->uid  = st->st_uid; 
	file->gid  = st->st_gid;
	file->mode = st->st_mode;
	return 0;
}

static int shm_truncate(vfs_node_t *node,size_t size){
	shm_file_t *file = node->private_inode;
	if(size > file->size){
		size_t current_size = PAGE_ALIGN_UP(file->size);
		while(current_size < size){
			shm_block_t *block = kmalloc(sizeof(shm_block_t));
			block->block = pmm_allocate_page();
			list_append(&file->blocks, &block->node);
			current_size += PAGE_SIZE;
		}
	} else {
		size_t off = 0;
		// FIXME : not safe
		// we free before going to next
		foreach(node,&file->blocks){
			shm_block_t *block = (shm_block_t*)node;
			if(off >= size){
				pmm_free_page(block->block);
				kfree(block);
			}
			off += PAGE_SIZE;
		}
	}
	file->size = size;
	return 0;
}

static void shm_cleanup(vfs_node_t *node){
	shm_file_t *file = node->private_inode;
	file->ref_count--;
	if(file->ref_count == 0){
		shm_destroy(file);
	}
}
//op for the shmfs root

static int shmfs_getattr(vfs_node_t *node,struct stat *st){
	(void)node;
	st->st_mode = 0777;
	return 0;
}

int shmfs_readdir(vfs_fd_t *node,unsigned long index,struct dirent *dirent){
	(void)node;
	if(index == 0){
		strcpy(dirent->d_name,".");
		return 0;
	}

	if(index == 1){
		strcpy(dirent->d_name,"..");
		return 0;
	}

	index -=2;
	foreach(node,& shm_files){
		if(!index){
			shm_file_t *file = (shm_file_t*)node;
			strcpy(dirent->d_name,file->name);
			return 0;
		}
		index--;
	}
	return -ENOENT;
}

static shm_file_t *shmfs_file_from_name(const char *name){
	foreach(node, &shm_files){
		shm_file_t *file = (shm_file_t*)node;
		if (!strcmp(file->name,name)) return file;
	}
	return NULL;
}

vfs_ops_t shmfs_ops = {
	.mmap = shm_mmap,
	.getattr = shm_getattr,
	.setattr = shm_setattr,
	.truncate = shm_truncate,
	.cleanup = shm_cleanup,
};

static vfs_node_t *shmfs_lookup(vfs_node_t *node,const char *name){
	(void)node;
	shm_file_t *file = shmfs_file_from_name(name);
	if(!file)return NULL;
	file->ref_count++;
	vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
	memset(n,0,sizeof(vfs_node_t));
	n->flags = VFS_FILE;
	n->private_inode = file;
	n->ops = &shmfs_ops;
	return n;
}

static int shmfs_create(vfs_node_t *node,const char *name,mode_t perm,long flags,void *arg){
	(void)node;
	(void)arg;
	//no dir in shmfs
	if(flags & VFS_DIR)return -EINVAL;

	if(shmfs_file_from_name(name))return -EEXIST;

	shm_file_t *file = kmalloc(sizeof(shm_file_t));
	memset(file,0,sizeof(shm_file_t));
	strcpy(file->name,name);
	file->ref_count = 1;
	file->mode = perm;
	init_list(&file->blocks);
	list_append(&shm_files, &file->node);
	
	return 0;
}


static int shmfs_unlink(vfs_node_t *node,const char *name){
	(void)node;
	shm_file_t *file = shmfs_file_from_name(name);
	if(!file)return -ENOENT;

	list_remove(&shm_files, &file->node);
	file->ref_count--;

	if(file->ref_count == 0){
		shm_destroy(file);
	}
	return 0;
}

vfs_ops_t shm_root_ops = {
	.readdir = shmfs_readdir,
	.getattr = shmfs_getattr,
	.unlink  = shmfs_unlink,
	.lookup  = shmfs_lookup,
	.create  = shmfs_create,
};

void init_shm(void){
	kstatusf("init shared memory subsystem ... ");
	init_list(&shm_files);

	shmfs_root = kmalloc(sizeof(vfs_node_t));
	memset(shmfs_root,0,sizeof(vfs_node_t));
	shmfs_root->flags = VFS_DIR;
	shmfs_root->ops = &shm_root_ops;

	vfs_register_fs(&shm_fs);
	kok();
}
