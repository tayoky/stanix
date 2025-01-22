#include "tmpfs.h"
#include "kheap.h"
#include "string.h"
#include "print.h"
#include "asm.h"

static tmpfs_inode *new_inode(const char *name,uint64_t flags){
	tmpfs_inode *inode = kmalloc(sizeof(tmpfs_inode));
	inode->child = NULL;
	inode->brother = NULL;
	inode->children_count = 0;
	inode->buffer_size = 0;
	inode->buffer = kmalloc(0);
	inode->flags = flags;
	inode->parent = NULL;
	strcpy(inode->name,name);
}

static vfs_node*inode2node(tmpfs_inode *inode){
	vfs_node *node = kmalloc(sizeof(vfs_node));
	node->private_inode = (void *)inode;

	if(inode->flags & TMPFS_FLAGS_DIR){
		node->finddir = tmpfs_finddir;
		node->readdir = tmpfs_readdir;
		node->create = tmpfs_create;
		node->mkdir = tmpfs_mkdir;
		node->unlink = tmpfs_unlink;
	}

	if(inode->flags & TMPFS_FLAGS_FILE){
		node->read = tmpfs_read;
		node->write = tmpfs_write;
		node->truncate = tmpfs_truncate;
	}

	node->close = tmpfs_close;
}

void init_tmpfs(){
	kstatus("init tmpfs... ");
	if(vfs_mount("tmp",new_tmpfs())){
		kfail();
		halt();
	}
	kok();
	vfs_node *tmp_root = vfs_open("tmp:/");
	
	vfs_mkdir("tmp:/sys",000);
	vfs_close(tmp_root);
	vfs_create("tmp:/sys/log",777);
	vfs_node *sys_log_file = vfs_open("tmp:/sys/log");
	kdebugf("vfs_node : 0x%lx\n",sys_log_file);
	char test[] = "tmpfs succefull init";
	vfs_write(sys_log_file,test,0,strlen(test)+1);
	vfs_close(sys_log_file);
}
vfs_node *new_tmpfs(){
	return inode2node(new_inode("root",TMPFS_FLAGS_DIR));
}


vfs_node *tmpfs_finddir(vfs_node *node,const char *name){
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

uint64_t tmpfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	//if the read is out of bound make it smaller
	if(offset + count > inode->buffer_size){
		if(offset >= inode->buffer_size){
			return 0;
		}
		count = inode->buffer_size - offset;
	}

	memcpy(buffer,inode->buffer,count);

	return count;
}

uint64_t tmpfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

	//if the write is out of bound make the file bigger
	if(offset + count > inode->buffer_size){
		tmpfs_truncate(node,offset + count);
	}
	memcpy(inode->buffer,buffer,count);
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
	
	return 0;
}


struct dirent *tmpfs_readdir(vfs_node *node,uint64_t index){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;

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

}


int tmpfs_create(vfs_node *node,const char *name,int perm){
	kdebugf("creating file %s\n",name);
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	tmpfs_inode *child_inode = new_inode(name,TMPFS_FLAGS_FILE);
	child_inode->parent = inode;
	inode->children_count++;
	child_inode->brother = inode->child;
	inode->child = child_inode;
	return 0;
}
int tmpfs_mkdir(vfs_node *node,const char *name,int perm){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	tmpfs_inode *child_inode = new_inode(name,TMPFS_FLAGS_DIR);
	child_inode->parent = inode;
	inode->children_count++;
	child_inode->brother = inode->child;
	inode->child = child_inode;
	return 0;
}