#include <kernel/scheduler.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/proc.h>
#include <kernel/vfs.h>

long strtol(const char *str, char **end,int base);

struct dirent *proc_root_readdir(vfs_node *node,uint64_t index){
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
	foreach(node,proc_list){
		if(!index){
            process *proc = node->value;
			struct dirent *ret = kmalloc(sizeof(struct dirent));
			sprintf(ret->d_name,"%d",proc->pid);
			return ret;
		}
		index--;
	}
    return NULL;
}

int proc_getattr(vfs_node *node,struct stat *st){
    process *proc = node->private_inode;
    st->st_uid  = proc->euid;
    st->st_gid  = proc->egid;
    st->st_mode = 0x555;
    return 0;
}

vfs_node *proc_root_lookup(vfs_node *root,const char *name){
    (void)root;
    char *end;
    pid_t pid = strtol(name,&end,10);
    if(end == name)return NULL;
    process *proc = pid2proc(pid);
    if(!proc)return NULL;

    vfs_node *node = kmalloc(sizeof(vfs_node));
    memset(node,0,sizeof(vfs_node));
    node->private_inode = proc;
    node->flags   = VFS_DIR;
    node->getattr = proc_getattr;
    return node;
}

int proc_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)data;
	(void)source;
	(void)flags;

	vfs_node *node = kmalloc(sizeof(vfs_node));
    memset(node,0,sizeof(vfs_node));
    node->flags   = VFS_DIR;
    node->readdir = proc_root_readdir;
    node->lookup  = proc_root_lookup;

	return vfs_mount(target,node);
}

vfs_filesystem proc_fs = {
    .name = "proc",
    .mount = proc_mount,
};

void init_proc(void){
    kdebugf("init proc fs ...");
    vfs_register_fs(&proc_fs);
    kok();
}