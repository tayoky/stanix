#include <kernel/scheduler.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/proc.h>
#include <kernel/vfs.h>
#include <errno.h>

long strtol(const char *str, char **end,int base);

int proc_root_readdir(vfs_node *node,unsigned long index,struct dirent *dirent){
    (void)node;
	if(index == 0){
		strcpy(dirent->d_name,".");
		return 0;
	}

	if(index == 1){
		strcpy(dirent->d_name,"..");
		return 0;
	}

    if(index == 2){
		strcpy(dirent->d_name,"self");
		return 0;
	}

    index -=3;
	foreach(node,proc_list){
		if(!index){
            process_t *proc = node->value;
			sprintf(dirent->d_name,"%d",proc->pid);
			return 0;
		}
		index--;
	}
    return -ENOENT;
}

int proc_getattr(vfs_node *node,struct stat *st){
    process_t *proc = node->private_inode;
    st->st_uid  = proc->euid;
    st->st_gid  = proc->egid;
    st->st_mode = 0x555;
    return 0;
}

int proc_readdir(vfs_node *node,unsigned long index,struct dirent *dirent){
    (void)node;
    static char *content[] = {
        ".",
        "..",
        "cwd",
    };

    if(index >= sizeof(content)/sizeof(*content))return -ENOENT;

    strcpy(dirent->d_name,content[index]);
    return 0;
}

int proc_link_getattr(vfs_node *node, struct stat *st) {
    process_t *proc = node->private_inode;
    st->st_uid  = proc->euid;
    st->st_gid  = proc->egid;
    st->st_mode = 0777;
    return 0;
}

ssize_t proc_self_readlink(vfs_node *node, char *buffer, size_t count) {
    (void)node;
    char buf[128];
    sprintf(buf, "/proc/%ld", get_current_proc()->pid);
    if (count > strlen(buf) + 1) count = strlen(buf) + 1;
    memcpy(buffer, buf, count);
    return count;
}

ssize_t proc_cwd_readlink(vfs_node *node, char *buffer, size_t count) {
    process_t *proc = node->private_inode;
    if (count > strlen(proc->cwd_path) + 1) count = strlen(proc->cwd_path) + 1;
    memcpy(buffer, proc->cwd_path, count);
    return count;
}

vfs_node *proc_lookup(vfs_node *root,const char *name) {
    process_t *proc = root->private_inode;
    vfs_node *node = kmalloc(sizeof(vfs_node));
    memset(node,0,sizeof(vfs_node));
    node->private_inode = proc;
    if (!strcmp(name, "cwd")) {
        node->flags    = VFS_LINK;
        node->getattr  = proc_link_getattr;
        node->readlink = proc_cwd_readlink;
        return node;
    }
    kfree(node);
    return NULL;
}

vfs_node *proc_root_lookup(vfs_node *root,const char *name){
    (void)root;
    if (!strcmp(name ,"self")) {
        vfs_node *node = kmalloc(sizeof(vfs_node));
        memset(node,0,sizeof(vfs_node));
        node->private_inode = get_current_proc();
        node->flags    = VFS_LINK;
        node->getattr  = proc_link_getattr;
        node->readlink = proc_self_readlink;
        return node;
    }
    char *end;
    pid_t pid = strtol(name,&end,10);
    if(end == name)return NULL;
    process_t *proc = pid2proc(pid);
    if(!proc)return NULL;

    vfs_node *node = kmalloc(sizeof(vfs_node));
    memset(node,0,sizeof(vfs_node));
    node->private_inode = proc;
    node->flags   = VFS_DIR;
    node->getattr = proc_getattr;
    node->readdir = proc_readdir;
    node->lookup  = proc_lookup;
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