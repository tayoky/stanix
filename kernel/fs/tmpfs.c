#include <kernel/tmpfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/time.h>
#include <kernel/list.h>
#include <errno.h>

static tmpfs_inode *new_inode(uint64_t flags){
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

static vfs_node *inode2node(tmpfs_inode *inode){
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->private_inode = (void *)inode;
	node->flags = 0;

	if(inode->flags & TMPFS_FLAGS_DIR){
		node->lookup     = tmpfs_lookup;
		node->readdir    = tmpfs_readdir;
		node->create     = tmpfs_create;
		node->unlink     = tmpfs_unlink;
		node->link       = tmpfs_link;
		node->flags |= VFS_DIR;
	}

	if(inode->flags & TMPFS_FLAGS_FILE){
		node->read       = tmpfs_read;
		node->write      = tmpfs_write;
		node->truncate   = tmpfs_truncate;
		node->flags |= VFS_FILE;
	}

	node->close    = tmpfs_close;
	node->getattr  = tmpfs_getattr;
	node->setattr  = tmpfs_setattr;
	inode->open_count++;
	return node;
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
	kstatus("init tmpfs... ");
	vfs_register_fs(&tmpfs);
	kok();
}
vfs_node *new_tmpfs(){
	return inode2node(new_inode(TMPFS_FLAGS_DIR));
}


vfs_node *tmpfs_lookup(vfs_node *node,const char *name){
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
ssize_t tmpfs_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

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

ssize_t tmpfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	//if the write is out of bound make the file bigger
	if(offset + count > inode->buffer_size){
		tmpfs_truncate(node,offset + count);
	}

	//update mtime
	inode->mtime = NOW();

	memcpy((void *)((uintptr_t)inode->buffer + offset),buffer,count);
	return count;
}

int tmpfs_truncate(vfs_node *node,size_t size){
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

int tmpfs_unlink(vfs_node *node,const char *name){
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

int tmpfs_link(vfs_node *,const char*,vfs_node*,const char*){
	return 0;
}

struct dirent *tmpfs_readdir(vfs_node *node,uint64_t index){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	//update atime
	inode->atime = NOW();

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
	foreach(node,inode->entries){
		if(!index){
			tmpfs_dirent *entry = node->value;
			struct dirent *ret = kmalloc(sizeof(struct dirent));
			strcpy(ret->d_name,entry->name);
			return ret;
		}
		index--;
	}
	return NULL;
}

void tmpfs_close(vfs_node *node){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	inode->open_count--;
	if(inode->open_count == 0 && inode->link_count == 0){
		//nobody use it
		free_inode(inode);
	}
}

int tmpfs_create(vfs_node *node,const char *name,mode_t perm,long flags){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	//turn vfs flag into tmpfs flags
	uint64_t inode_flag = 0;
	if(flags & VFS_FILE){
		inode_flag |= TMPFS_FLAGS_FILE;
	}
	if(flags & VFS_DIR){
		inode_flag |= TMPFS_FLAGS_DIR;
	}

	//create new inode
	tmpfs_inode *child_inode = new_inode(inode_flag);
	child_inode->link_count = 1;
	child_inode->parent = inode;
	child_inode->perm = perm;

	//create new entry
	tmpfs_dirent *entry = kmalloc(sizeof(tmpfs_dirent));
	memset(entry,0,sizeof(tmpfs_dirent));
	strcpy(entry->name,name);
	entry->inode = child_inode;
	list_append(inode->entries,entry);

	return 0;
}

int tmpfs_setattr(vfs_node *node,struct stat *st){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	inode->perm         = st->st_mode & 0xffff;
	inode->owner        = st->st_uid;   
	inode->group_owner  = st->st_gid;
	inode->atime        = st->st_atime;
	inode->mtime        = st->st_mtime;
	inode->ctime        = st->st_ctime;
	return 0;
}

int tmpfs_getattr(vfs_node *node,struct stat *st){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	st->st_size        = inode->buffer_size;
	st->st_mode        = inode->perm;
	st->st_uid         = inode->owner;
	st->st_gid         = inode->group_owner;
	st->st_atime       = inode->atime;
	st->st_mtime       = inode->mtime;
	st->st_ctime       = inode->ctime;
	st->st_nlink       = inode->link_count;
	
	//simulate fake blocks of 512 bytes
	//because blocks don't exist on tmpfs
	st->st_blksize = 512;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize;
	return 0;
}