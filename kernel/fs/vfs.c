#include "vfs.h"
#include "kernel.h"
#include "print.h"
#include "kheap.h"
#include "string.h"
#include "time.h"
#include <stddef.h>
#include <errno.h>

static inline int strequ(const char *str1,const char *str2){
	while (*str1 == *str2){
		if(!*str1){
			return 1;
		}
		str1++;
		str2++;
	}
	return 0;
}

static inline vfs_mount_point *vfs_get_mount_point(const char *drive){
	vfs_mount_point *current = kernel->first_mount_point;

	while (current){
		if(strequ(current->name,drive)){
			return current;
		}
		current = current->next;
	}
	return NULL;
}

static inline vfs_node *vfs_get_root(const char *name){
	vfs_mount_point *mount_point = vfs_get_mount_point(name);
	if(!mount_point){
		return NULL;
	}

	return mount_point->root;
}

void init_vfs(void){
	kstatus("init vfs... ");
	kernel->first_mount_point = NULL;
	kok();
}


int vfs_mount(const char *name,vfs_node *mounting_node){
	if(!mounting_node)return -EINVAL;

	//check if something is aready mount
	if(vfs_get_mount_point(name)){
		return -EEXIST;
	}

	vfs_mount_point *mount_point = kmalloc(sizeof(vfs_mount_point));
	memset(mount_point,0,sizeof(vfs_mount_point));
	strcpy(mount_point->name,name);
	mount_point->root = mounting_node;

	mount_point->next = kernel->first_mount_point;
	kernel->first_mount_point = mount_point;
	return 0;
}
ssize_t vfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	if(node->read){
		return node->read(node,(void *)buffer,offset,count);
	} else {
		if(node->flags & VFS_DIR){
			return -EISDIR;
		}
		return -EBADF;
	}
}

ssize_t vfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	if(node->write){
		return node->write(node,(void *)buffer,offset,count);
	} else {
		if(node->flags & VFS_DIR){
			return -EISDIR;
		}
		return -EBADF;
	}
}


vfs_node *vfs_lookup(vfs_node *node,const char *name){
	if(node->lookup){
		vfs_node *child = node->lookup(node,(char *)name);
		if(child){
			child->ref_count = 1;
		}
		return child;
	} else {
		return NULL;
	}
}

void vfs_close(vfs_node *node){
	node->ref_count --;

	if(node->ref_count > 0){
		return;
	}

	if(node->mount_point && node->mount_point->root == node){
		//don't close it it's root !!!
		return;
	}

	if(node->close){
		node->close(node);
	}

	kfree(node);
}

int vfs_create_dev(const char *path,device_op *op,void *dev_inode){
	int ret = -ENOENT;

	//make a copy of the path
	char *parent = strdup(path);
	char *child = parent + strlen(path) - 2;
	while(*child != '/'){
		child --;
		if(child <= parent){
			break;
		}
	}
	*child = '\0';
	child++;

	//open the parent
	vfs_node *node = vfs_open(parent,VFS_WRITEONLY);
	if(!node){
		goto vfs_create_dev_error;
	}

	//call create on the parent
	if(node->create_dev){
		ret = node->create_dev(node,(char *)child,op,dev_inode);
	} else {
		ret = -ENOTDIR;
	}

	//close and free
	vfs_close(node);
	vfs_create_dev_error:
	kfree(parent);
	return ret;
}

int vfs_create(const char *path,int perm,uint64_t flags){
	//TODO : check if aready exist
	int ret = -ENOENT;

	//make a copy of the path
	char *parent = strdup(path);
	char *child = parent + strlen(path) - 2;
	while(*child != '/'){
		child --;
		if(child <= parent){
			break;
		}
	}
	*child = '\0';
	child++;

	//open the parent
	vfs_node *node = vfs_open(parent,VFS_WRITEONLY);
	if(!node){
		//cant free or GD for some reason
		return -ENOENT;
	}
	
	//call create on the parent
	if(node->create){
		ret = node->create(node,child,perm,flags);
	} else {
		ret = -ENOTDIR;
	}

	//close and free
	vfs_close(node);
	kfree(parent);
	return ret;
}

int vfs_mkdir(const char *path,int perm){
	return vfs_create(path,perm,VFS_DIR);
}

int vfs_unlink(vfs_node *node,const char *name){
	if(node->unlink){
		return node->unlink(node,(char *)name);
	} else {
		if(!(node->flags & VFS_DIR)){
			return -ENOTDIR;
		}
		return -EBADF;
	}
}

struct dirent *vfs_readdir(vfs_node *node,uint64_t index){
	if(node->readdir){
		return node->readdir(node,index);
	} else {
		return NULL;
	}
}

int vfs_truncate(vfs_node *node,size_t size){
	if(node->truncate){
		return node->truncate(node,size);
	} else {
		if(node->flags & VFS_DIR){
			return -EISDIR;
		}
		return -EBADF;
	}
}

int vfs_chmod(vfs_node *node,mode_t perm){
	if(!node->chmod){
		return -1;
	}
	int ret = node->chmod(node,perm);
	if(!ret){
		node->perm = perm;
	}
	return ret;
}

int vfs_chown(vfs_node *node,uid_t owner,gid_t group_owner){
	if(!node->chown){
		return -1;
	}
	int ret = node->chown(node,owner,group_owner);
	if(!ret){
		node->owner = owner;
		node->group_owner = group_owner;
	}
	return ret;
}

int vfs_ioctl(vfs_node *node,uint64_t request,void *arg){
	if(node->ioctl){
		return node->ioctl(node,request,arg);
	} else {
		return -1;
	}
}

vfs_node *vfs_dup(vfs_node *node){
	//well we can copy a the void so ...
	//we can copy an non existant node
	if(!node) return NULL;
	node->ref_count++;
	return node;
}

int vfs_sync(vfs_node *node){
	//make sure we can actually sync
	if(!node->sync){
		return -EIO; //should be another error ... but what ???
	}
	return node->sync(node);
}

vfs_node *vfs_open(const char *path,uint64_t flags){
	//don't open for nothing
	if((!flags & VFS_READONLY )&& (!(flags &  VFS_WRITEONLY))){
		return NULL;
	}

	//first parse the path
	char *new_path = strdup(path);

	//path are like "drive:/folder/file.extention"
	char *drive_separator = new_path;
	while(*drive_separator != ':'){
		if(!(*drive_separator)){
			//path invalid
			return NULL;
		}
		drive_separator++;
	}

	*drive_separator = '\0';
	char *drive = new_path;
	new_path = drive_separator+1;

	uint64_t path_depth = 0;

	//find the path depth
	for(int i = 0; new_path[i] ; i++){
		//only if it's a path separator
		if(new_path[i] != '/') {
			continue;
		}
		new_path[i] = '\0';
		if(new_path[i + 1]) {
			path_depth++;
		}
	}

	
	vfs_node *current_node = vfs_get_root(drive);

	char *current_dir = &new_path[1];
	for (uint64_t i = 0; i < path_depth; i++){
		if(!current_node)goto open_error;
		vfs_node *next_node = vfs_lookup(current_node,current_dir);
		vfs_close(current_node);
		current_node = next_node;
		current_dir += strlen(current_dir) + 1;
	}
	
	if(!current_node)goto open_error;

	current_node->mount_point = vfs_get_mount_point(drive);
	kfree(drive);

	///update modify / access time
	if((flags & VFS_WRITEONLY) || (flags & VFS_READWRITE)){
		current_node->mtime = NOW();
	}
	if((flags & VFS_READONLY) || (flags & VFS_READWRITE)){
		current_node->atime = NOW();
	}

	return current_node;

	open_error:
	kfree(drive);
	return NULL;
}