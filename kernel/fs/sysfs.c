#include <kernel/vfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/sysfs.h>
#include <kernel/print.h>
#include <kernel/pmm.h>
#include <errno.h>

static vfs_node_t *sysfs_inode2vnode(sysfs_inode *inode);

vfs_node_t *sysfs_root;

static sysfs_inode *new_sysfs_inode(void){
    sysfs_inode *inode = kmalloc(sizeof(sysfs_inode));
    memset(inode,0,sizeof(sysfs_inode));
    init_list(&inode->child);
    return inode;
}

void sysfs_register(const char *name,vfs_node_t *node){
    sysfs_inode *inode = new_sysfs_inode();
    inode->linked_node = node;
    inode->name = strdup(name);
    list_append(&((sysfs_inode *)sysfs_root->private_inode)->child, &inode->node);
}

int sysfs_readdir(vfs_fd_t *fd,unsigned long index,struct dirent *dirent){
    sysfs_inode *inode = fd->private;
	if(index == 0){
		strcpy(dirent->d_name,".");
		return 0;
	}

	if(index == 1){
		strcpy(dirent->d_name,"..");
		return 0;
	}

    index -=2;
	foreach(node, &inode->child){
		if(!index){
            sysfs_inode *entry = (sysfs_inode*)node;
			strcpy(dirent->d_name,entry->name);
			return 0;
		}
		index--;
	}
    return -ENOENT;
}


vfs_node_t *sysfs_lookup(vfs_node_t *node,const char *name){
    sysfs_inode *inode = node->private_inode;
	foreach(node, &inode->child){
        sysfs_inode *entry = (sysfs_inode*)node;
		if(!strcmp(name,entry->name)){
            return sysfs_inode2vnode(entry);
        }
	}
    return NULL;
}

vfs_ops_t sysfs_ops = {
    .readdir = sysfs_readdir,
    .lookup  = sysfs_lookup,
};

static vfs_node_t *sysfs_inode2vnode(sysfs_inode *inode){
    if(inode->linked_node)return vfs_dup_node(inode->linked_node);
    vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
    memset(node,0,sizeof(vfs_node_t));
    node->private_inode = inode;
    node->flags = VFS_DIR;
    node->ops = &sysfs_ops;

    return node;
}

int sysfs_mount(const char *source,const char *target,unsigned long flags,const void *data){
    (void)source;
    (void)flags;
    (void)data;
    return vfs_mount(target,sysfs_root);
}

static vfs_filesystem_t sys_fs = {
    .name = "sysfs",
    .mount = sysfs_mount,
};

static ssize_t mem_read(vfs_fd_t *fd,void *buf,off_t off,size_t count){
    (void)fd;
    char str[512];
    sprintf(str,"total : %ld\nused  : %ld\n", pmm_get_total_pages() * PAGE_SIZE, pmm_get_used_pages() * PAGE_SIZE);
    if((size_t)off > strlen(str))return 0;
    if(off + count > strlen(str))count = strlen(str) - off;
    memcpy(buf,&str[off],count);
    return count;
}

static vfs_ops_t mem_ops = {
    .read = mem_read,
};

void init_sysfs(void){
    kstatusf("init sysfs ... ");
    sysfs_root = sysfs_inode2vnode(new_sysfs_inode());

    // simple /sys/mem
    vfs_node_t *mem = kmalloc(sizeof(vfs_node_t));
    memset(mem,0,sizeof(vfs_node_t));
    mem->flags = VFS_FILE;
    mem->ops  = &mem_ops;
    mem->ref_count = 1;
    sysfs_register("mem",mem);

    vfs_register_fs(&sys_fs);
    kok();
}
