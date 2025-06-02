#include <kernel/tmpfs.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/asm.h>
#include <kernel/time.h>

static tmpfs_inode *new_inode(const char *name,uint64_t flags){
	tmpfs_inode *inode = kmalloc(sizeof(tmpfs_inode));
	memset(inode,0,sizeof(tmpfs_inode));
	inode->child = NULL;
	inode->brother = NULL;
	inode->children_count = 0;
	inode->buffer_size = 0;
	inode->buffer = kmalloc(0);
	inode->flags = flags;
	inode->parent = NULL;

	//set times
	inode->atime = NOW();
	inode->ctime = NOW();
	inode->mtime = NOW();

	strcpy(inode->name,name);
	return inode;
}

static vfs_node*inode2node(tmpfs_inode *inode){
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->private_inode = (void *)inode;
	node->flags = 0;

	if(inode->flags & TMPFS_FLAGS_DIR){
		node->lookup    = tmpfs_lookup;
		node->readdir    = tmpfs_readdir;
		node->create     = tmpfs_create;
		node->unlink     = tmpfs_unlink;
		node->flags |= VFS_DIR;
	}

	if(inode->flags & TMPFS_FLAGS_FILE){
		node->read       = tmpfs_read;
		node->write      = tmpfs_write;
		node->truncate   = tmpfs_truncate;
		node->flags |= VFS_FILE;
		node->size = inode->buffer_size;
	}

	node->close = tmpfs_close;

	//copy metadata
	node->perm        = inode->perm;
	node->owner       = inode->owner;
	node->group_owner = inode->group_owner;
	return node;
}

void init_tmpfs(){
	kstatus("init tmpfs... ");
	if(vfs_mount("/tmp",new_tmpfs())){
		kfail();
		halt();
	}
	kok();
	vfs_node *tmp_root = vfs_open("/tmp/",VFS_READONLY);
	
	vfs_mkdir("/tmp/sys",000);
	vfs_close(tmp_root);
	vfs_create("/tmp/sys/log",0x777,VFS_FILE);
	vfs_node *sys_log_file = vfs_open("/tmp/sys/log",VFS_WRITEONLY);
	char test[] = "tmpfs succefull init";
	vfs_write(sys_log_file,test,0,strlen(test)+1);
	vfs_close(sys_log_file);
}
vfs_node *new_tmpfs(){
	return inode2node(new_inode("root",TMPFS_FLAGS_DIR));
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
	
	tmpfs_inode *current_inode = inode->child;
	for (uint64_t i = 0; i < inode->children_count + 1; i++){
		if(current_inode == NULL)return NULL;
		if(!strcmp(current_inode->name,name)){
			break;
		}
		current_inode = current_inode->brother;
	}

	if(current_inode == NULL)return NULL;
	return inode2node(current_inode);
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

	memcpy(buffer,(void *)((uint64_t)inode->buffer) + offset,count);

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

	memcpy((void *)((uint64_t)inode->buffer) + offset,buffer,count);
	return count;
}

int tmpfs_truncate(vfs_node *node,size_t size){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	char *new_buffer = kmalloc(size);

	if(inode->buffer_size > size){
		memcpy(new_buffer,inode->buffer,inode->buffer_size);
	} else {
		memcpy(new_buffer,inode->buffer,size);
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
	tmpfs_inode *current_inode = inode->child;
	tmpfs_inode *prev_inode = NULL;

	for (uint64_t i = 0; i < inode->children_count; i++){
		if(current_inode == NULL)return -1;
		if(!strcmp(inode->name,name)){
			break;
		}
		prev_inode = current_inode;
		current_inode = current_inode->brother;
	}
	if(current_inode == NULL)return -1;

	if(prev_inode){
		prev_inode->brother = current_inode->brother;
	} else {
		inode->child = current_inode->brother;
	}
	
	kfree(current_inode->buffer);
	kfree(current_inode);

	inode->children_count--;
	
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
	if(index >= inode->children_count){
		//out of bound LOL
		return NULL;
	}
	tmpfs_inode *current_inode = inode->child;
	for (uint64_t i = 0; i < index; i++){
		if(!current_inode){
			//weird error should nerver happen
			return NULL;
		}
		current_inode = current_inode->brother;
	}

	struct dirent *ret = kmalloc(sizeof(struct dirent));
	strcpy(ret->d_name,current_inode->name);

	return ret;
}

void tmpfs_close(vfs_node *node){
	(void)node;
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
	tmpfs_inode *child_inode = new_inode(name,inode_flag);
	child_inode->parent = inode;
	inode->children_count++;
	child_inode->brother = inode->child;
	inode->child = child_inode;

	inode->perm = perm;
	return 0;
}

int tmpfs_chmod(vfs_node *node,mode_t perm){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	inode->perm = perm;
	return 0;
}
int tmpfs_chown(vfs_node *node,uid_t owner,gid_t group_owner){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	inode->owner = owner;
	inode->group_owner = group_owner;
	return 0;
}

int tmpfs_sync(vfs_node *node){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	node->size        = inode->buffer_size;
	node->perm        = inode->perm;
	node->owner       = inode->owner;
	node->group_owner = inode->group_owner;
	return 0;
}