#include <kernel/vfs.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/memseg.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <kernel/device.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <poll.h>

//TODO : make this process specific
vfs_node_t *root;

list_t*fs_types;

void init_vfs(void){
	kstatusf("init vfs... ");
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

int vfs_mount_on(vfs_node_t *mount_point, vfs_node_t *local_root) {
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

int vfs_mountat(vfs_node_t *at,const char *name,vfs_node_t *local_root){
	//first open the mount point or create it
	vfs_node_t *mount_point = vfs_get_node_at(at,name,O_RDWR);
	if(!mount_point){
		return -ENOENT;
	}

	int ret = vfs_mount_on(mount_point, local_root);

	vfs_close_node(mount_point);

	return ret;
}

int vfs_mount(const char *name,vfs_node_t *local_root){
	return vfs_mountat(NULL,name,local_root);
}

int vfs_unmount(const char *path){
	return vfs_unmountat(NULL, path);
}

//TODO : we don't handle the case where a parent of the mount point get closed
//the local_root stay open but somebody try to open it's parent
int vfs_unmountat(vfs_node_t *at, const char *path){
	vfs_node_t *parent = vfs_get_node_at(at,path,O_PARENT);
	if(!parent){
		return -ENOENT;
	}

	// i think we don't handle trailling / very well
	const char *child = vfs_basename(path);
	
	//we can use vfs_lookup cause it don't folow mount point (only vfs_openat does)
	vfs_node_t *mount_point = vfs_lookup(parent,child);
	vfs_close_node(parent);
	if(!(mount_point->flags & VFS_MOUNT)){
		//not even a mount point
		return -EINVAL;
	}
	vfs_node_t *local_root = mount_point->linked_node;

	mount_point->flags  &= ~VFS_MOUNT;

	vfs_close_node(local_root);
	vfs_close_node(mount_point);
	return 0;
}

ssize_t vfs_read(vfs_fd_t *fd,void *buffer,uint64_t offset,size_t count){
	if (fd->type & VFS_DIR) {
		return -EISDIR;
	}
	if (fd->flags & O_WRONLY) {
		return -EBADF;
	}
	if (fd->ops->read) {
		return fd->ops->read(fd,buffer,offset,count);
	} else {
		return -EINVAL;
	}
}

ssize_t vfs_write(vfs_fd_t *fd,const void *buffer,uint64_t offset,size_t count){
	if (fd->type & VFS_DIR) {
		return -EISDIR;
	}
	if (!(fd->flags & (O_WRONLY | O_RDWR))) {
		return -EBADF;
	}
	if (fd->ops->write) {
		return fd->ops->write(fd,buffer,offset,count);
	} else {
		return -EINVAL;
	}
}

ssize_t vfs_readlink(vfs_node_t *node, char *buf, size_t bufsiz){
	if (!(node->flags & VFS_LINK)) {
			return -ENOLINK;
	}
	if (node->ops->readlink) {
		return node->ops->readlink(node,buf,bufsiz);
	} else {
		return -EIO;
	}
}

int vfs_wait_check(vfs_fd_t *fd,short type){
	if(fd->ops->wait_check){
		return fd->ops->wait_check(fd,type);
	} else {
		//by default report as ready for all request actions
		//so that stuff such as files are alaways ready
		return type;
	}
}

int vfs_wait(vfs_fd_t *fd,short type){
	if(fd->ops->wait){
		return fd->ops->wait(fd,type);
	} else {
		//mmmm...
		//how we land here ??
		return -EINVAL;
	}
}


vfs_node_t *vfs_lookup(vfs_node_t *node,const char *name){
	if (!(node->flags & VFS_DIR)) {
		return NULL;
	}
	
	//handle .. here so we can handle the parent of mount point
	if((!strcmp("..",name)) && node->parent){
		return vfs_dup_node(node->parent);
	}

	if((!strcmp(".",name))){
		return vfs_dup_node(node);
	}

	//first search in the directory cache
	for(vfs_node_t *current = node->child; current; current = current->brother){
		if(!strcmp(current->name,name)){
			current->ref_count++;
			return current;
		}
	}

	//it isen't chached
	//ask the fs for it

	if(node->ops->lookup){
		vfs_node_t *child = node->ops->lookup(node,name);
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

void vfs_close_node(vfs_node_t *node){
	if (!node) return;
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
			for(vfs_node_t *current = node->parent->child; current; current = current->brother){
				if(current->brother == node){
					current->brother = node->brother;
					break;
				}
			}
		}
		node->parent->childreen_count--;
	}

	vfs_node_t *parent = node->parent;
	if(parent == node){
		parent = NULL;
	}

	if(node->ops->cleanup){
		node->ops->cleanup(node);
	}

	kfree(node);

	//maybee we can close the parent too ?
	if(parent){
		if(parent->childreen_count == 0 && parent->ref_count == 0){
			parent->ref_count++;
			return vfs_close_node(parent);
		}
	}
}

int vfs_create(const char *path, int perm, long flags){
	return vfs_createat(NULL, path, perm, flags);
}

int vfs_createat(vfs_node_t *at, const char *path, int perm, long flags) {
	return vfs_createat_ext(at, path, perm, flags, NULL);
}

int vfs_create_ext(const char *path, int perm, long flags, void *arg){
	return vfs_createat_ext(NULL, path, perm, flags, arg);
}

int vfs_createat_ext(vfs_node_t *at,const char *path,int perm,long flags,void *arg){
	//open the parent
	vfs_node_t *parent = vfs_get_node_at(at, path, O_WRONLY | O_PARENT);
	if(!parent){
		return -ENOENT;
	}
	if (!(parent->flags & VFS_DIR)) {
		vfs_close_node(parent);
		return -ENOTDIR;
	}

	const char *child = vfs_basename(path);
	
	//call create on the parent
	int ret;
	if(parent->ops->create){
		ret = parent->ops->create(parent,child,perm,flags,arg);
	} else {
		ret = -EIO;
	}

	//close
	vfs_close_node(parent);
	return ret;
}

int vfs_unlink(const char *path){
	//open parent
	vfs_node_t *parent = vfs_get_node(path,O_WRONLY | O_PARENT);
	if(!parent){
		return -ENOENT;
	}
	if (!(parent->flags & VFS_DIR)) {
		vfs_close_node(parent);
		return -ENOTDIR;
	}

	const char *child = vfs_basename(path);

	//call unlink on the parent
	int ret;
	if(parent->ops->create){
		ret = parent->ops->unlink(parent,child);
	} else {
		ret = -EIO;
	}
	
	//cleanup
	vfs_close_node(parent);
	return ret;
}


int vfs_symlink(const char *target, const char *linkpath){
	//open parent
	vfs_node_t *parent = vfs_get_node(linkpath,O_WRONLY | O_PARENT);
	if(!parent){
		return -ENOENT;
	}
	if (!(parent->flags & VFS_DIR)) {
		vfs_close_node(parent);
		return -ENOTDIR;
	}

	const char *child  = vfs_basename(linkpath);
	int ret;
	if(parent->ops->symlink){
		ret = parent->ops->symlink(parent,child,target);
	} else {
		ret = -EIO;
	}

	vfs_close_node(parent);
	return ret;
}

int vfs_link(const char *src,const char *dest){
	//open parent
	vfs_node_t *parent_src = vfs_get_node(src,O_WRONLY | O_PARENT);
	if(!parent_src){
		return -ENOENT;
	}
	vfs_node_t *parent_dest = vfs_get_node(dest,O_WRONLY | O_PARENT);
	if(!parent_dest){
		vfs_close_node(parent_src);
		return -ENOENT;
	}
	if (!(parent_src->flags & VFS_DIR)) {
		vfs_close_node(parent_src);
		vfs_close_node(parent_dest);
		return -ENOTDIR;
	}
	if (!(parent_dest->flags & VFS_DIR)) {
		vfs_close_node(parent_src);
		vfs_close_node(parent_dest);
		return -ENOTDIR;
	}

	const char *child_src  = vfs_basename(src);
	const char *child_dest = vfs_basename(dest);

	//call link on the parents
	int ret;
	if(parent_src->ops->link){
		ret = parent_src->ops->link(parent_src,child_src,parent_dest,child_dest);
	} else {
		ret = -EIO;
	}
	
	//cleanup
	vfs_close_node(parent_src);
	vfs_close_node(parent_dest);
	return ret;
}

int vfs_readdir(vfs_fd_t *fd,unsigned long index,struct dirent *dirent){
	if (!(fd->type & VFS_DIR)) {
		return -ENOTDIR;
	}
	dirent->d_type = DT_UNKNOWN;
	dirent->d_ino  = 1; //some programs want non NULL inode
	if(fd->ops->readdir){
		return fd->ops->readdir(fd,index,dirent);
	} else {
		return -EINVAL;
	}
}

int vfs_chmod(vfs_node_t *node,mode_t perm){
	struct stat st;
	int ret = vfs_getattr(node,&st);
	if(ret < 0)return ret;
	st.st_mode = perm;
	return vfs_setattr(node,&st);
}

int vfs_chown(vfs_node_t *node,uid_t owner,gid_t group_owner){
	struct stat st;
	int ret = vfs_getattr(node,&st);
	if(ret < 0)return ret;
	st.st_uid = owner;
	st.st_gid = group_owner;
	return vfs_setattr(node,&st);
}

int vfs_getattr(vfs_node_t *node,struct stat *st) {
	memset(st,0,sizeof(struct stat));
	st->st_nlink = 1; //in case a driver forgot to set :D
	st->st_mode  = 0744; //default mode
	//make sure we can actually sync
	if (node->ops->getattr) {
		int ret = node->ops->getattr(node,st);
		if(ret < 0)return ret;
	}
	
	//file type to mode
	if (node->flags & VFS_FILE) {
		st->st_mode |= S_IFREG;
	} else if (node->flags & VFS_DIR) {
		st->st_mode |= S_IFDIR;
	} else if (node->flags & VFS_SOCK) {
		st->st_mode |= S_IFSOCK;
	} else if (node->flags & VFS_BLOCK) {
		st->st_mode |= S_IFBLK;
	} else if (node->flags & VFS_CHAR) {
		st->st_mode |= S_IFCHR;
	} else if (node->flags & VFS_LINK) {
		st->st_mode |= S_IFLNK;
	}
	return 0;
}

int vfs_setattr(vfs_node_t *node,struct stat *st){
	//make sure we can actually sync
	if(!node->ops->setattr){
		return -EINVAL; //should be another error ... but what ???
	}
	return node->ops->setattr(node,st);
}

int vfs_chroot(vfs_node_t *new_root){
	root = new_root;
	return 0;
}

vfs_node_t *vfs_get_node_at(vfs_node_t *at, const char *path, long flags){
	if (!at) {
		//if absolute relative to root else relative to cwd
		if(path[0] == '/' || path[0] == '\0'){
			return vfs_get_node_at(root,path,flags);
		}
		return vfs_get_node_at(get_current_proc()->cwd_node,path,flags);
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

	// we handle O_PARENT here
	if(flags & O_PARENT){
		//do we have a parent ?
		if(path_depth < 1){
			return NULL;
		}
		path_depth--;
	}

	vfs_node_t *current_node = vfs_dup_node(at);
	int loop_max = SYMLOOP_MAX;

	for (int i = 0; i < path_depth; i++){
		if(!current_node)return NULL;
		
		vfs_node_t *next_node = vfs_lookup(current_node,path_array[i]);
		//folow mount points and symlink
		while(next_node){
			if(next_node->flags & VFS_MOUNT){
				vfs_node_t *mount_point = next_node;
				next_node = vfs_dup_node(next_node->linked_node);
				vfs_close_node(mount_point);
				continue;
			}
			if((next_node->flags & VFS_LINK) && (!(flags & O_NOFOLLOW) || i < path_depth - 1)){
				//TODO : maybee cache linked node ?
				vfs_node_t *symlink = next_node;
				if(loop_max-- <= 0){
					vfs_close_node(symlink);
					return NULL;
				}
				char linkpath[PATH_MAX];
				ssize_t size;
				if((size = vfs_readlink(symlink,linkpath,sizeof(linkpath))) < 0){
					vfs_close_node(symlink);
					return NULL;
				}
				linkpath[size] = '\0';
				
				//TODO : prevent infinite recursion
				next_node = vfs_get_node(linkpath,flags);
				vfs_close_node(symlink);
			}
			break;
		}
		vfs_close_node(current_node);
		current_node = next_node;
	}
	
	if(!current_node)return NULL;
	
	return current_node;
}

vfs_fd_t *vfs_openat(vfs_node_t *at, const char *path, long flags) {
	vfs_node_t *node = vfs_get_node_at(at, path, flags);
	if (!node) return NULL;

	vfs_fd_t *fd = vfs_open_node(node, flags);
	vfs_close_node(node);
	return fd;
}

vfs_fd_t *vfs_open_node(vfs_node_t *node, long flags) {
	struct stat st;
	vfs_fd_t *fd = kmalloc(sizeof(vfs_fd_t));
	memset(fd, 0, sizeof(vfs_fd_t));
	vfs_getattr(node, &st);

	fd->ops       = node->ops;
	fd->private   = node->private_inode;
	fd->inode     = vfs_dup_node(node);
	fd->flags     = flags;
	fd->ref_count = 1;
	fd->type      = node->flags;

	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
		device_t *device = device_from_number(st.st_rdev);
		if (!device) goto error;
		fd->ops     = device->ops;
		fd->private = device;
	}

	if (fd->ops->open) {
		int ret = fd->ops->open(fd);
		if (ret < 0) {
			error:
			vfs_close_node(fd->inode);
			kfree(fd);
			return NULL;
		}
	}

	
	/// update modify / access time
	if (flags & O_RDWR) {
		st.st_mtime = NOW();
		st.st_atime = NOW();
	} else if (flags & O_WRONLY) {
		st.st_mtime = NOW();
	} else {
		st.st_atime = NOW();
	}
	vfs_setattr(node, &st);

	return fd;
}

void vfs_close(vfs_fd_t *fd) {
	fd->ref_count--;
	if (fd->ref_count > 0) return;
	vfs_close_node(fd->inode);

	if(fd->ops->close) {
		fd->ops->close(fd);
	}

	kfree(fd);
}

int vfs_user_perm(vfs_node_t *node, uid_t uid, gid_t gid) {
	struct stat st;
	vfs_getattr(node, &st);

	int is_other = 1;
	int perm = 0;
	if (uid == 0) {
		// root can read/write anything
		perm |= 06;
	}

	if (uid == st.st_uid) {
		is_other = 0;
		perm |= st.st_mode & 07;
	}
	if (gid == st.st_gid) {
		is_other = 0;
		perm |= (st.st_mode >> 3) & 07;
	}
	if (is_other) {
		perm |= (st.st_mode >> 6) & 07;
	}

	return perm;
}

int vfs_perm(vfs_node_t *node) {
	return vfs_user_perm(node, get_current_proc()->euid, get_current_proc()->egid);
}
