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
	inode->buffer_size = 1;
	inode->buffer = kmalloc(inode->buffer_size);
	inode->flags = flags;
}

static vfs_node*inode2node(tmpfs_inode *inode){
	vfs_node *node = kmalloc(sizeof(vfs_node));
	node->private_inode = (void *)inode;

	if(inode->flags & TMPFS_FLAGS_DIR){
		node->finddir = tmpfs_finddir;
		node->create = tmpfs_create;
		node->mkdir = tmpfs_mkdir;
	}

	if(inode->flags & TMPFS_FLAGS_FILE){
		node->read = tmpfs_read;
		node->write = tmpfs_write;
	}

	node->close = tmpfs_close;
}

void init_tmpfs(){
	kstatus("init tmpfs... ");
	if(vfs_mount("tmp",new_tmpfs())){
		kfail();
		halt();
	}
	vfs_node *test = vfs_open("tmp:/");
	
	vfs_mkdir(test,"folder",777);
	kok();
}
vfs_node *new_tmpfs(){
	return inode2node(new_inode("root",TMPFS_FLAGS_DIR));
}


vfs_node *tmpfs_finddir(vfs_node *node,const char *name){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	tmpfs_inode *current_inode = inode->child;
	for (uint64_t i = 0; i < inode->children_count; i++){
		if(current_inode == NULL)return NULL;
		if(!strcmp(inode->name,name)){
			break;
		}
		current_inode = current_inode->brother;
	}

	if(current_inode == NULL)return NULL;
	return inode2node(current_inode);
}

uint64_t tmpfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	return count;
}

uint64_t tmpfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	return count;
}

void tmpfs_close(vfs_node *node){

}


int tmpfs_create(vfs_node *node,const char *name,int perm){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	tmpfs_inode *child_inode = new_inode(name,TMPFS_FLAGS_FILE);
	inode->children_count++;
	child_inode->brother = inode->child;
	inode->child = child_inode;
}
int tmpfs_mkdir(vfs_node *node,const char *name,int perm){
	tmpfs_inode *inode = (tmpfs_inode *)node->private_inode;
	tmpfs_inode *child_inode = new_inode(name,TMPFS_FLAGS_DIR);
	inode->children_count++;
	child_inode->brother = inode->child;
	inode->child = child_inode;
}