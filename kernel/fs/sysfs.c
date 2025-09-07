#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/sysfs.h>
#include <kernel/print.h>
#include <kernel/kernel.h>

static vfs_node *sysfs_inode2vnode(sysfs_inode *inode);

vfs_node *sysfs_root;

static sysfs_inode *new_sysfs_inode(void){
    sysfs_inode *inode = kmalloc(sizeof(sysfs_inode));
    memset(inode,0,sizeof(sysfs_inode));
    inode->child = new_list();
    return inode;
}

void sysfs_register(const char *name,vfs_node *node){
    sysfs_inode *inode = new_sysfs_inode();
    inode->linked_node = node;
    inode->name = strdup(name);
    list_append(((sysfs_inode *)sysfs_root->private_inode)->child,inode);
}

struct dirent *sysfs_readdir(vfs_node *node,uint64_t index){
    sysfs_inode *inode = node->private_inode;
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
	foreach(node,inode->child){
		if(!index){
            sysfs_inode *entry = node->value;
			struct dirent *ret = kmalloc(sizeof(struct dirent));
			strcpy(ret->d_name,entry->name);
			return ret;
		}
		index--;
	}
    return NULL;
}


vfs_node *sysfs_lookup(vfs_node *node,const char *name){
    sysfs_inode *inode = node->private_inode;
	foreach(node,inode->child){
        sysfs_inode *entry = node->value;
		if(!strcmp(name,entry->name)){
            return sysfs_inode2vnode(entry);
        }
	}
    return NULL;
}

static vfs_node *sysfs_inode2vnode(sysfs_inode *inode){
    if(inode->linked_node)return vfs_dup(inode->linked_node);
    vfs_node *node = kmalloc(sizeof(vfs_node));
    memset(node,0,sizeof(vfs_node));
    node->private_inode = inode;
    node->flags = VFS_DIR;
    node->readdir = sysfs_readdir;
    node->lookup  = sysfs_lookup;

    return node;
}

int sysfs_mount(const char *source,const char *target,unsigned long flags,const void *data){
    (void)source;
    (void)flags;
    (void)data;
    return vfs_mount(target,sysfs_root);
}

vfs_filesystem sys_fs = {
    .name = "sysfs",
    .mount = sysfs_mount,
};

ssize_t mem_read(vfs_node *node,void *buf,uint64_t off,size_t count){
    (void)node;
    char str[512];
    sprintf(str,"total : %ld\nused  : %ld\n",kernel->total_memory,kernel->used_memory);
    if(off > strlen(str))return 0;
    if(off + count > strlen(str))count = strlen(str) - off;
    memcpy(buf,&str[off],count);
    return count;
}

void init_sysfs(void){
    kstatus("init sysfs ... ");
    sysfs_root = sysfs_inode2vnode(new_sysfs_inode());

    // simple /sys/mem
    vfs_node *mem = kmalloc(sizeof(vfs_node));
    memset(mem,0,sizeof(vfs_node));
    mem->flags = VFS_DEV | VFS_BLOCK;
    mem->read  = mem_read;
    mem->ref_count = 1;
    sysfs_register("mem",mem);

    vfs_register_fs(&sys_fs);
    kok();
}