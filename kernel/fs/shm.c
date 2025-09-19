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

list *shm_files;
vfs_node *shmfs_root;

//shared memory subsystem
int shmfs_mount(const char *source,const char *target,unsigned long flags,const void *data){
    (void)source;
    (void)flags;
    (void)data;
    return vfs_mount(target,shmfs_root);
}

vfs_filesystem shm_fs = {
    .name = "shmfs",
    .mount = shmfs_mount,
};

static void shm_destroy(shm_file *file){
    foreach(node,file->blocks){
        uintptr_t block = (uintptr_t)node->value;
        pmm_free_page(block);
    }
    free_list(file->blocks);
    kfree(file);
}

static void shm_unmap(memseg *seg){
    (void)seg;
    shm_file *file = seg->private_data;
    file->ref_count--;
    if(file->ref_count == 0){
        shm_destroy(file);
    }
}

static void *shm_mmap(vfs_node *node,void *addr,size_t lenght,uint64_t prot,int flags,off_t offset){
    //why would you map shm as private ????
	if(!(flags & MAP_SHARED)){
		return (void *)-EINVAL;
	}


    shm_file *file = node->private_inode;

    offset = PAGE_ALIGN_DOWN(offset);
    if(offset > (off_t)file->size)return (void*)-EINVAL;
    if(offset + lenght >= file->size)lenght = file->size - offset;
	if(lenght == 0)return (void *)-EINVAL;

	memseg *seg = memseg_create(get_current_proc(),(uintptr_t)addr,lenght,prot,flags);
	if(!seg)return (void *)-EEXIST;
	seg->unmap = shm_unmap;

    file->ref_count++;
    seg->private_data = file;

    off_t off = 0;
    uintptr_t vaddr = seg->addr;
    foreach(node,file->blocks){
        if(off >= offset && (size_t)off <= offset + lenght){
            if(vaddr >= seg->addr + seg->size)break;
            map_page(get_addr_space(),(uintptr_t)node->value,vaddr,prot);
            vaddr += PAGE_SIZE;
        }
        off += PAGE_SIZE;
    }

    return (void *)seg->addr;
}

//op for the actual shm files   
static int shm_getattr(vfs_node *node,struct stat *st){
    shm_file *file = node->private_inode;
    st->st_uid  = file->uid;
    st->st_gid  = file->gid;
    st->st_mode = file->mode;
    st->st_size = file->size;
    st->st_blksize = PAGE_SIZE;
    st->st_blocks = PAGE_ALIGN_UP(st->st_size);
    return 0;
}

static int shm_setattr(vfs_node *node,struct stat *st){
    shm_file *file = node->private_inode;
    file->uid  = st->st_uid; 
    file->gid  = st->st_gid;
    file->mode = st->st_mode;
    return 0;
}

static int shm_truncate(vfs_node *node,size_t size){
    shm_file *file = node->private_inode;
    if(size > file->size){
        size_t current_size = PAGE_ALIGN_UP(file->size);
        while(current_size < size){
            list_append(file->blocks,(void*)pmm_allocate_page());
            current_size += PAGE_SIZE;
        }
    } else {
        size_t off = 0;
        foreach(node,file->blocks){
            if(off >= size){
                uintptr_t block = (uintptr_t)node->value;
                pmm_free_page(block);
            }
            off += PAGE_SIZE;
        }
    }
    file->size = size;
    return 0;
}

static void shm_close(vfs_node *node){
    shm_file *file = node->private_inode;
    file->ref_count--;
    if(file->ref_count == 0){
        shm_destroy(file);
    }
}
//op for the shmfs root

static int shmfs_getattr(vfs_node *node,struct stat *st){
    (void)node;
    st->st_mode = 0777;
    return 0;
}

static struct dirent *shmfs_readdir(vfs_node *node,uint64_t index){
    (void)node;
	if(index == 0){
		struct dirent *ret = kmalloc(sizeof(struct dirent));
		strcpy(ret->d_name,".");
		return ret;
	}

	if(index == 1){
		struct dirent *ret = kmalloc(sizeof(struct dirent));
		strcpy(ret->d_name,"..");
		return ret;
	}

    index -=2;
	foreach(node,shm_files){
		if(!index){
            shm_file *file = node->value;
			struct dirent *ret = kmalloc(sizeof(struct dirent));
			strcpy(ret->d_name,file->name);
			return ret;
		}
		index--;
	}
    return NULL;
}

static shm_file *shmfs_file_from_name(const char *name){
	foreach(node,shm_files){
        shm_file *file = node->value;
		if(!strcmp(file->name,name))return file;
	}
    return NULL;
}

static vfs_node *shmfs_lookup(vfs_node *node,const char *name){
    (void)node;
    shm_file *file = shmfs_file_from_name(name);
    if(!file)return NULL;
    file->ref_count++;
    vfs_node *n = kmalloc(sizeof(vfs_node));
    memset(n,0,sizeof(vfs_node));
    n->flags = VFS_DEV | VFS_BLOCK;
    n->getattr  = shm_getattr;
    n->setattr  = shm_setattr;
    n->truncate = shm_truncate;
    n->close    = shm_close;
    n->mmap     = shm_mmap;
    n->private_inode = file;
    return n;
}

static int shmfs_create(vfs_node *node,const char *name,mode_t perm,long flags){
    (void)node;
    //no dir in shmfs
    if(flags & VFS_DIR)return -EINVAL;

    if(shmfs_file_from_name(name))return -EEXIST;

    shm_file *file = kmalloc(sizeof(shm_file));
    memset(file,0,sizeof(shm_file));
    strcpy(file->name,name);
    file->ref_count = 1;
    file->mode = perm;
    file->blocks = new_list();
    list_append(shm_files,file);
    
    return 0;
}


static int shmfs_unlink(vfs_node *node,const char *name){
    (void)node;
    shm_file *file = shmfs_file_from_name(name);
    if(!file)return -ENOENT;

    list_remove(shm_files,file);
    file->ref_count--;

    if(file->ref_count == 0){
        shm_destroy(file);
    }
    return 0;
}

void init_shm(void){
    kstatus("init shared memory subsystem ... ");
    shm_files = new_list();

    shmfs_root = kmalloc(sizeof(vfs_node));
    memset(shmfs_root,0,sizeof(vfs_node));
    shmfs_root->flags = VFS_DIR;
    shmfs_root->readdir = shmfs_readdir;
    shmfs_root->getattr = shmfs_getattr;
    shmfs_root->unlink  = shmfs_unlink;
    shmfs_root->lookup  = shmfs_lookup;
    shmfs_root->create = shmfs_create;

    vfs_register_fs(&shm_fs);
    kok();
}