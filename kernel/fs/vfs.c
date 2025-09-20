#include <kernel/vfs.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/memseg.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <poll.h>

//TODO : make this process specific
vfs_node *root;

list *fs_types;

void init_vfs(void){
	kstatus("init vfs... ");
	root = NULL;
	fs_types  = new_list();
	kok();
}

void vfs_register_fs(vfs_filesystem *fs){
	list_append(fs_types,fs);
}

void vfs_unregister_fs(vfs_filesystem *fs){
	list_remove(fs_types,fs);
}

int vfs_auto_mount(const char *source,const char *target,const char *filesystemtype,unsigned long mountflags,const void *data){
	foreach(node,fs_types){
		vfs_filesystem *fs = node->value;
		if(!strcmp(fs->name,filesystemtype)){
			if(!fs->mount){
				return -ENODEV;
			}
			return fs->mount(source,target,mountflags,data);
		}
	}

	return -ENODEV;
}

int vfs_mount(const char *name,vfs_node *local_root){
	//first open the mount point or create it
	vfs_node *mount_point = vfs_open(name,VFS_READWRITE);
	if(!mount_point){
		if(vfs_mkdir(name,0777)){
			return -ENOENT;
		}
		mount_point = vfs_open(name,VFS_READWRITE);
		if(!mount_point){
			return -ENOENT;
		}
	}

	//something is aready mounted ?
	if(mount_point->flags & VFS_MOUNT){
		return -EBUSY;
	}

	mount_point->linked_node = local_root;

	//make a new ref to the local root and mount point to prevent it from being close
	local_root->ref_count++;
	mount_point->ref_count++;

	mount_point->flags |= VFS_MOUNT;

	local_root->parent = mount_point->parent;
	return 0;
}


//TODO : we don't handle the case where a parent of the mount point get closed
//the local_root stay open but somebody try to open it's parent
int vfs_unmount(const char *path){
	vfs_node *parent = vfs_open(path,VFS_PARENT);
	if(!parent){
		return -ENOENT;
	}

	const char *child = path + strlen(path) - 1;
	//path of directory might finish with '/' like /tmp/dir/
	if(*child == '/')child--;
	while(*child != '/'){
		child--;
		if(child < path){
			break;
		}
	}
	child++;
	
	//we can use vfs_lookup cause it don't folow mount point (only vfs_openat does)
	vfs_node *mount_point = vfs_lookup(parent,path);
	vfs_close(parent);
	if(!(mount_point->flags & VFS_MOUNT)){
		//not even a mount point
		return -EINVAL;
	}
	vfs_node *local_root = mount_point->linked_node;

	mount_point->flags  &= ~VFS_MOUNT;

	vfs_close(local_root);
	vfs_close(mount_point);
	return 0;
}

ssize_t vfs_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	if(node->read){
		return node->read(node,(void *)buffer,offset,count);
	} else {
		if(node->flags & VFS_DIR){
			return -EISDIR;
		}
		return -EBADF;
	}
}

ssize_t vfs_write(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	if(node->write){
		return node->write(node,(void *)buffer,offset,count);
	} else {
		if(node->flags & VFS_DIR){
			return -EISDIR;
		}
		return -EBADF;
	}
}

ssize_t vfs_readlink(vfs_node *node,char *buf,size_t bufsiz){
	if(node->readlink){
		return node->readlink(node,buf,bufsiz);
	} else {
		if(node->flags & VFS_LINK){
			return -EIO;
		}
		return -ENOLINK;
	}
}

int vfs_wait_check(vfs_node *node,short type){
	if(node->wait_check){
		return node->wait_check(node,type);
	} else {
		//by default report as ready for all request actions
		//so that stuff such as files are alaways ready
		return type;
	}
}

int vfs_wait(vfs_node *node,short type){
	if(node->wait){
		return node->wait(node,type);
	} else {
		//mmmm...
		//how we land here ??
		return -EINVAL;
	}
}


vfs_node *vfs_lookup(vfs_node *node,const char *name){
	//handle .. here so we can handle the parent of mount point
	if((!strcmp("..",name)) && node->parent){
		return vfs_dup(node->parent);
	}

	if((!strcmp(".",name))){
		return vfs_dup(node);
	}

	//first search in the directory cache
	for(vfs_node *current = node->child; current; current = current->brother){
		if(!strcmp(current->name,name)){
			current->ref_count++;
			return current;
		}
	}

	//it isen't chached
	//ask the fs for it

	if(node->lookup){
		vfs_node *child = node->lookup(node,(char *)name);
		if(!child){
			return NULL;
		}
		if(!child->ref_count)child->ref_count = 1;
		
		//link it in the directories cache
		child->parent = node;
		child->brother = node->child;
		node->child = child;
		node->childreen_count++;
		child->child = NULL;
		child->childreen_count = 0;

		strcpy(child->name,name);
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

	if(node->flags & VFS_MOUNT || node == root){
		//don't close a mount point
		return;
	}

	//if childreen can't close
	if(node->childreen_count > 0){
		return;
	}

	//we can finaly close
	//but before remove the node from the directories cache
	if(node->parent && node->parent != node){
		if(node->parent->child == node){
			//special case for the first child
			node->parent->child = node->brother;
		} else {
			for(vfs_node *current = node->parent->child; current; current = current->brother){
				if(current->brother == node){
					current->brother = node->brother;
					break;
				}
			}
		}
		node->parent->childreen_count--;
	}

	vfs_node *parent = node->parent;
	if(parent == node){
		parent = NULL;
	}

	if(node->close){
		node->close(node);
	}

	kfree(node);

	//maybee we can close the parent too ?
	if(parent){
		if(parent->childreen_count == 0 && parent->ref_count == 0){
			parent->ref_count++;
			return vfs_close(parent);
		}
	}
}

//basename without modyfing anything
static const char *vfs_basename(const char *path){
	const char *base = path + strlen(path) - 1;
	//path of directory might finish with '/' like /tmp/dir/
	if(*base == '/')base--;
	while(*base != '/'){
		base--;
		if(base < path){
			break;
		}
	}
	base++;
	return base;
}

int vfs_create(const char *path,int perm,uint64_t flags){
	//open the parent
	vfs_node *parent = vfs_open(path,VFS_WRITEONLY | VFS_PARENT);
	if(!parent){
		return -ENOENT;
	}

	const char *child = vfs_basename(path);
	
	//call create on the parent
	int ret;
	if(parent->create){
		ret = parent->create(parent,child,perm,flags);
	} else {
		ret = -ENOTDIR;
	}

	//close
	vfs_close(parent);
	return ret;
}

int vfs_mkdir(const char *path,int perm){
	return vfs_create(path,perm,VFS_DIR);
}

int vfs_unlink(const char *path){
	//open parent
	vfs_node *parent = vfs_open(path,VFS_WRITEONLY | VFS_PARENT);
	if(!parent){
		return -ENOENT;
	}

	const char *child = vfs_basename(path);

	//call unlink on the parent
	int ret;
	if(parent->create){
		ret = parent->unlink(parent,child);
	} else {
		ret = -ENOTDIR;
	}
	
	//cleanup
	vfs_close(parent);
	return ret;
}


int vfs_symlink(const char *target, const char *linkpath){
	//open parent
	vfs_node *parent = vfs_open(linkpath,VFS_WRITEONLY | VFS_PARENT);
	if(!parent){
		return -ENOENT;
	}

	const char *child  = vfs_basename(linkpath);
	int ret;
	if(parent->symlink){
		ret = parent->symlink(parent,child,target);
	} else if(parent->flags & VFS_DIR){
		ret = -EIO;
	} else {
		ret = -ENOTDIR;
	}

	vfs_close(parent);
	return ret;
}

int vfs_link(const char *src,const char *dest){
	//open parent
	vfs_node *parent_src = vfs_open(src,VFS_WRITEONLY | VFS_PARENT);
	if(!parent_src){
		return -ENOENT;
	}
	vfs_node *parent_dest = vfs_open(dest,VFS_WRITEONLY | VFS_PARENT);
	if(!parent_dest){
		return -ENOENT;
	}

	const char *child_src  = vfs_basename(src);
	const char *child_dest = vfs_basename(dest);

	//call link on the parents
	int ret;
	if(parent_src->link){
		ret = parent_src->link(parent_src,child_src,parent_dest,child_dest);
	} else {
		ret = -ENOTDIR;
	}
	
	//cleanup
	vfs_close(parent_src);
	vfs_close(parent_dest);
	return ret;
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
	struct stat st;
	int ret = vfs_getattr(node,&st);
	if(ret < 0)return ret;
	st.st_mode = perm;
	return vfs_setattr(node,&st);
}

int vfs_chown(vfs_node *node,uid_t owner,gid_t group_owner){
	struct stat st;
	int ret = vfs_getattr(node,&st);
	if(ret < 0)return ret;
	st.st_uid = owner;
	st.st_gid = group_owner;
	return vfs_setattr(node,&st);
}

int vfs_ioctl(vfs_node *node,uint64_t request,void *arg){
	if(node->ioctl){
		return node->ioctl(node,request,arg);
	} else {
		return -EINVAL;
	}
}


int vfs_mmap(vfs_node *node,off_t offset,struct memseg_struct *seg){
	if(node->mmap){
		return node->mmap(node,offset,seg);
	} else {
		//TODO : file mapping
		return -EINVAL;
	}
}

vfs_node *vfs_dup(vfs_node *node){
	//well we can copy a the void so ...
	//we can copy an non existant node
	if(!node) return NULL;
	node->ref_count++;
	return node;
}

int vfs_getattr(vfs_node *node,struct stat *st){
	memset(st,0,sizeof(struct stat));
	st->st_nlink = 1; //in case a driver forgot to set :D
	//make sure we can actually sync
	if(node->getattr){
		int ret = node->getattr(node,st);
		if(ret < 0)return ret;
	}
	
	//file type to mode
	if(node->flags & VFS_FILE){
		st->st_mode |= S_IFREG;
	}
	if(node->flags & VFS_DIR){
		st->st_mode |= S_IFDIR;
	}
	if(node->flags & VFS_DEV){
		if(node->flags & VFS_BLOCK){
			st->st_mode |= S_IFBLK;
		} else if(node->flags & VFS_CHAR){
			st->st_mode |= S_IFCHR;
		} else {
			st->st_mode |= S_IFBLK;
			st->st_mode |= S_IFCHR;
		}
	}
	if(node->flags & VFS_LINK){
		st->st_mode |= S_IFLNK;
	}
	return 0;
}

int vfs_setattr(vfs_node *node,struct stat *st){
	//make sure we can actually sync
	if(!node->setattr){
		return -EIO; //should be another error ... but what ???
	}
	return node->setattr(node,st);
}

int vfs_chroot(vfs_node *new_root){
	root = new_root;
	return 0;
}

vfs_node *vfs_open(const char *path,uint64_t flags){
	//if absolute relative to root else relative to cwd
	if(path[0] == '/' || path[0] == '\0'){
		return vfs_openat(root,path,flags);
	}
	return vfs_openat(get_current_proc()->cwd_node,path,flags);
}


vfs_node *vfs_openat(vfs_node *at,const char *path,uint64_t flags){
	//don't open for nothing
	if((!flags & VFS_READONLY )&& (!(flags &  VFS_WRITEONLY))){
		return NULL;
	}

	//we are going to modify it
	char new_path[strlen(path) + 1];
	strcpy(new_path,path);

	//first parse the path
	int path_depth = 0;
	char last_is_sep = 1;
	char *path_array[64]; //hardcoded maximum identation level

	for(int i = 0; new_path[i] ; i++){
		//only if it's a path separator
		if(new_path[i] == '/') {
			new_path[i] = '\0';
			last_is_sep = 1;
			continue;
		}

		if(last_is_sep){
			path_array[path_depth] = &new_path[i];
			path_depth++;
			last_is_sep = 0;
		}
	}

	//we handle VFS_PARENT here
	if(flags & VFS_PARENT){
		//do we have a parent ?
		if(path_depth < 1){
			return NULL;
		}
		path_depth--;
	}

	vfs_node *current_node = vfs_dup(at);
	int loop_max = SYMLOOP_MAX;

	for (int i = 0; i < path_depth; i++){
		if(!current_node)return NULL;
		
		vfs_node *next_node = vfs_lookup(current_node,path_array[i]);
		//folow mount points and symlink
		while(next_node){
			if(next_node->flags & VFS_MOUNT){
				vfs_node *mount_point = next_node;
				next_node = vfs_dup(next_node->linked_node);
				vfs_close(mount_point);
				continue;
			}
			if((next_node->flags & VFS_LINK) && (!(flags & VFS_NOFOLOW) || i < path_depth - 1)){
				//TODO : maybee cache linked node ?
				vfs_node *symlink = next_node;
				if(loop_max-- <= 0){
					vfs_close(symlink);
					return NULL;
				}
				char linkpath[PATH_MAX];
				ssize_t size;
				if((size = vfs_readlink(symlink,linkpath,sizeof(linkpath))) < 0){
					vfs_close(symlink);
					return NULL;
				}
				linkpath[size] = '\0';
				
				//TODO : prevent infinite recursion
				next_node = vfs_open(linkpath,flags);
				vfs_close(symlink);
			}
			break;
		}
		vfs_close(current_node);
		current_node = next_node;
	}
	
	if(!current_node)return NULL;

	///update modify / access time
	struct stat st;
	vfs_getattr(current_node,&st);
	if((flags & VFS_WRITEONLY) || (flags & VFS_READWRITE)){
		st.st_mtime = NOW();
	}
	if((flags & VFS_READONLY) || (flags & VFS_READWRITE)){
		st.st_atime = NOW();
	}
	vfs_setattr(current_node,&st);
	
	return current_node;
}