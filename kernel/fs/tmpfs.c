#include <kernel/tmpfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <errno.h>

static vfs_node_t *inode2node(tmpfs_inode *inode);

#define INODE_NUMBER(inode) ((ino_t)((uintptr_t)inode) - KHEAP_START)

#define IS_DEV(flags) (flags & (TMPFS_FLAGS_CHAR | TMPFS_FLAGS_BLOCK))

static tmpfs_inode *new_inode(long flags){
	tmpfs_inode *inode = kmalloc(sizeof(tmpfs_inode));
	memset(inode,0,sizeof(tmpfs_inode));
	inode->buffer_size = 0;
	inode->buffer = kmalloc(0);
	inode->flags = flags;
	inode->parent = NULL;
	inode->entries = new_list();
	inode->link_count = 1;

	//set times
	inode->atime = NOW();
	inode->ctime = NOW();
	inode->mtime = NOW();
	return inode;
}

static void free_inode(tmpfs_inode *inode){
	kfree(inode->buffer);
	free_list(inode->entries);
	kfree(inode);
}


int tmpfs_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)data;
	(void)source;
	(void)flags;

	return vfs_mount(target,new_tmpfs());
}

vfs_filesystem tmpfs = {
	.name = "tmpfs",
	.mount = tmpfs_mount,
};

void init_tmpfs(){
	kstatusf("init tmpfs... ");
	vfs_register_fs(&tmpfs);
	kok();
}

vfs_node_t *new_tmpfs(){
	tmpfs_inode *root_inode = new_inode(TMPFS_FLAGS_DIR);
	root_inode->link_count = 0; //so it get freed when the tmpfs is unmounted
	return inode2node(root_inode);
}


vfs_node_t *tmpfs_lookup(vfs_node_t *node,const char *name){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	if(!strcmp(name,".")){
		return inode2node(inode);
	}

	if(!strcmp(name,"..")){
		if(inode->parent)
		return inode2node(inode->parent);
		return inode2node(inode);
	}

	foreach(node,inode->entries){
		tmpfs_dirent *entry = node->value;
		if(!strcmp(name,entry->name)){
			return inode2node(entry->inode);
		}
	}

	return NULL;
}

ssize_t tmpfs_read(vfs_fd_t *fd,void *buffer,uint64_t offset,size_t count){
	tmpfs_inode *inode = (tmpfs_inode *)fd->private;

	//if the read is out of bound make it smaller
	if(offset + count > inode->buffer_size){
		if(offset >= inode->buffer_size){
			return 0;
		}
		count = inode->buffer_size - offset;
	}

	//update atime
	inode->atime = NOW();

	memcpy(buffer,(void *)((uintptr_t)inode->buffer + offset),count);

	return count;
}

int tmpfs_truncate(vfs_node_t *node,size_t size){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	char *new_buffer = kmalloc(size);

	if(inode->buffer_size > size){
		memcpy(new_buffer,inode->buffer,size);
	} else {
		memcpy(new_buffer,inode->buffer,inode->buffer_size);
	}

	kfree(inode->buffer);

	inode->buffer_size = size;
	inode->buffer = new_buffer;

	//update mtime
	inode->mtime = NOW();

	return 0;
}

ssize_t tmpfs_write(vfs_fd_t *fd,void *buffer,uint64_t offset,size_t count){
	tmpfs_inode *inode = (tmpfs_inode *)fd->private;

	//if the write is out of bound make the file bigger
	if(offset + count > inode->buffer_size){
		tmpfs_truncate(fd->inode, offset + count);
	}

	//update mtime
	inode->mtime = NOW();

	memcpy((void *)((uintptr_t)inode->buffer + offset),buffer,count);
	return count;
}

int tmpfs_unlink(vfs_node_t *node,const char *name){
	kdebugf("unlink %s\n",name);
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	
	tmpfs_dirent *entry = NULL;
	foreach(node,inode->entries){
		tmpfs_dirent *cur_entry = node->value;
		if(!strcmp(cur_entry->name,name)){
			entry = cur_entry;
			break;
		}
	}

	if(!entry)return ENOENT;

	list_remove(inode->entries,entry);
	if((--entry->inode->link_count) == 0 && entry->inode->open_count == 0){
		//nobody uses it we can free
		free_inode(entry->inode);
	}
	kfree(entry);

	return 0;
}

static int tmpfs_exist(tmpfs_inode *inode,const char *name){
	foreach(node,inode->entries){
		tmpfs_dirent *entry = node->value;
		if(!strcmp(name,entry->name)){
			return 1;
		}
	}
	return 0;
}

int tmpfs_link(vfs_node_t *parent_src,const char *src,vfs_node_t *parent_dest,const char *dest){
	kdebugf("link %s %s\n",src,dest);
	tmpfs_inode *parent_src_inode  = (tmpfs_inode *)parent_src->private_inode;
	tmpfs_inode *parent_dest_inode = (tmpfs_inode *)parent_dest->private_inode;

	if(tmpfs_exist(parent_dest_inode,dest))return -EEXIST;

	tmpfs_inode *src_inode = NULL;
	foreach(node,parent_src_inode->entries){
		tmpfs_dirent *entry = node->value;
		if(!strcmp(entry->name,src)){
			src_inode = entry->inode;
			break;
		}
	}
	if(!src_inode)return -ENOENT;

	//create new entry
	src_inode->link_count++;
	tmpfs_dirent *entry = kmalloc(sizeof(tmpfs_dirent));
	strcpy(entry->name,dest);
	entry->inode = src_inode;
	list_append(parent_dest_inode->entries,entry);
	return 0;
}


int tmpfs_symlink(vfs_node_t *node,const char *name,const char *target){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	if(tmpfs_exist(inode,name))return -EEXIST;

	tmpfs_inode *symlink = new_inode(TMPFS_FLAGS_LINK);
	kfree(symlink->buffer);
	symlink->buffer_size = strlen(target);
	symlink->buffer = strdup(target);

	//create new entry
	tmpfs_dirent *entry = kmalloc(sizeof(tmpfs_dirent));
	strcpy(entry->name,name);
	entry->inode = symlink;
	list_append(inode->entries,entry);

	return 0;
}

ssize_t tmpfs_readlink(vfs_node_t *node,char *buf,size_t bufsize){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	if(bufsize > inode->buffer_size)bufsize = inode->buffer_size;

	memcpy(buf,inode->buffer,bufsize);
	return bufsize;
}


int tmpfs_readdir(vfs_fd_t *fd,unsigned long index,struct dirent *dirent){
	tmpfs_inode *inode = (tmpfs_inode *)fd->private;

	//update atime
	inode->atime = NOW();

	if(index == 0){
		strcpy(dirent->d_name,".");
		dirent->d_ino = INODE_NUMBER(inode);
		dirent->d_type = DT_DIR;
		return 0;
	}

	if(index == 1){
		strcpy(dirent->d_name,"..");
		dirent->d_ino = INODE_NUMBER(inode->parent);
		dirent->d_type = DT_DIR;
		return 0;
	}

	index -=2;
	foreach(node,inode->entries){
		if(!index){
			tmpfs_dirent *entry = node->value;
			strcpy(dirent->d_name,entry->name);
			dirent->d_ino = INODE_NUMBER(entry->inode);
			if (entry->inode->flags & TMPFS_FLAGS_DIR) {
				dirent->d_type = DT_DIR;
			} else if (entry->inode->flags & TMPFS_FLAGS_FILE) {
				dirent->d_type = DT_REG;
			} else if (entry->inode->flags & TMPFS_FLAGS_LINK) {
				dirent->d_type = DT_LNK;
			} else if (entry->inode->flags & TMPFS_FLAGS_SOCK) {
				dirent->d_type = DT_SOCK;
			} else if (entry->inode->flags & TMPFS_FLAGS_CHAR) {
				dirent->d_type = DT_CHR;
			} else if (entry->inode->flags & TMPFS_FLAGS_BLOCK) {
				dirent->d_type = DT_BLK;
			}
			return 0;
		}
		index--;
	}
	return -ENOENT;
}

void tmpfs_cleaunup(vfs_node_t *node){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	inode->open_count--;
	if(inode->open_count == 0 && inode->link_count == 0){
		//nobody use it
		free_inode(inode);
	}
}

int tmpfs_create(vfs_node_t *node,const char *name,mode_t perm,long flags,void *arg){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	if(tmpfs_exist(inode,name))return -EEXIST;

	//turn vfs flag into tmpfs flags
	uint64_t inode_flag = 0;
	if(flags & VFS_FILE){
		inode_flag |= TMPFS_FLAGS_FILE;
	}
	if(flags & VFS_SOCK){
		inode_flag |= TMPFS_FLAGS_SOCK;
	}
	if(flags & VFS_DIR){
		inode_flag |= TMPFS_FLAGS_DIR;
	}
	if(flags & VFS_CHAR){
		inode_flag |= TMPFS_FLAGS_CHAR;
	}
	if(flags & VFS_BLOCK){
		inode_flag |= TMPFS_FLAGS_BLOCK;
	}

	//create new inode
	tmpfs_inode *child_inode = new_inode(inode_flag);
	child_inode->link_count = 1;
	child_inode->parent = inode;
	child_inode->perm = perm;
	child_inode->data = arg;

	//create new entry
	tmpfs_dirent *entry = kmalloc(sizeof(tmpfs_dirent));
	memset(entry,0,sizeof(tmpfs_dirent));
	strcpy(entry->name,name);
	entry->inode = child_inode;
	list_append(inode->entries,entry);

	return 0;
}

int tmpfs_setattr(vfs_node_t *node,struct stat *st){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	inode->perm         = st->st_mode & 0xffff;
	inode->owner        = st->st_uid;   
	inode->group_owner  = st->st_gid;
	inode->atime        = st->st_atime;
	inode->mtime        = st->st_mtime;
	inode->ctime        = st->st_ctime;
	inode->dev          = st->st_dev;
	return 0;
}

int tmpfs_getattr(vfs_node_t *node,struct stat *st){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	st->st_size        = inode->buffer_size;
	st->st_mode        = inode->perm;
	st->st_uid         = inode->owner;
	st->st_gid         = inode->group_owner;
	st->st_atime       = inode->atime;
	st->st_mtime       = inode->mtime;
	st->st_ctime       = inode->ctime;
	st->st_nlink       = inode->link_count;
	st->st_dev         = inode->dev;
	st->st_ino         = INODE_NUMBER(inode); // fake an inode number
	
	//simulate fake blocks of 512 bytes
	//because blocks don't exist on tmpfs
	st->st_blksize = 512;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize;
	return 0;
}

vfs_ops_t tmfps_ops = {
	.lookup     = tmpfs_lookup,
	.readdir    = tmpfs_readdir,
	.create     = tmpfs_create,
	.unlink     = tmpfs_unlink,
	.link       = tmpfs_link,
	.symlink    = tmpfs_symlink,
	.readlink   = tmpfs_readlink,
	.read       = tmpfs_read,
	.write      = tmpfs_write,
	.truncate   = tmpfs_truncate,
	.getattr    = tmpfs_getattr,
	.setattr    = tmpfs_setattr,
	.cleanup    = tmpfs_cleaunup,
};

static vfs_node_t *inode2node(tmpfs_inode *inode){
	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node,0,sizeof(vfs_node_t));
	node->private_inode = (void *)inode;
	node->flags = 0;
	node->ops = &tmfps_ops;

	if (inode->flags & TMPFS_FLAGS_DIR) {
		node->flags |= VFS_DIR;
	}

	if (inode->flags & TMPFS_FLAGS_FILE) {
		node->flags |= VFS_FILE;
	}

	if (inode->flags & TMPFS_FLAGS_SOCK) {
		node->private_inode2 = inode->data;
		node->flags |= VFS_SOCK;
	}

	if (IS_DEV(inode->flags)) {
		node->flags |= VFS_DEV;
		if (inode->flags & TMPFS_FLAGS_CHAR) {
			node->flags |= VFS_CHAR;
		}
		if (inode->flags & TMPFS_FLAGS_BLOCK) {
			node->flags |= VFS_BLOCK;
		}
	}

	inode->open_count++;
	return node;
}
