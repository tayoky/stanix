#include "vfs.h"
#include "kernel.h"
#include "print.h"
#include "kheap.h"
#include "string.h"
#include <stddef.h>

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

static inline vfs_node *vfs_get_root(const char *name){
	vfs_mount_point *current = kernel->first_mount_point;

	while (current){
		if(strequ(current->name,name)){
			return current->root;
		}
		current = current->next;
	}
	return NULL;
}

void init_vfs(void){
	kstatus("init vfs... ");
	kernel->first_mount_point = NULL;
	kok();
}

//todo add check for if something is aready mount at this place
int vfs_mount(const char *name,vfs_node *mounting_node){
	if(!mounting_node)return -1;

	vfs_mount_point *mount_point = kmalloc(sizeof(vfs_mount_point));
	memset(mount_point,0,sizeof(vfs_mount_point));
	strcpy(mount_point->name,name);
	mount_point->root = mounting_node;

	mount_point = kernel->first_mount_point;
	kernel->first_mount_point = mount_point;
	return 0;
}

uint64_t vfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	if(node->read){
		return node->read(node,(void *)buffer,offset,count);
	} else {
		return -1;
	}
}

uint64_t vfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	if(node->write){
		return node->write(node,(void *)buffer,offset,count);
	} else {
		return -1;
	}
}


vfs_node *vfs_finddir(vfs_node *node,const char *name){
	if(node->finddir){
		return node->finddir(node,(char *)name);
	} else {
		return NULL;
	}
}

void vfs_close(vfs_node *node){
	if(node->close){
		node->close(node);
	}

	kfree(node);
}

vfs_node *vfs_open(const char *path){
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
	kprintf("drive : %s\n",drive);

	uint64_t path_depth = 0;
	//first count the number of depth
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

    char **path_array = kmalloc(sizeof(char*) * path_depth);

    int j = 0;
    for(int i = 0; i < path_depth; i++){
        while(new_path[j]){
            j++;
        }
        path_array[i] = (char *) new_path + j + 1;
    }
    
	kdebugf("path depth : %lx\n",path_depth);

	vfs_node *local_root = vfs_get_root(drive);
	vfs_node *current_node = local_root;

	for (uint64_t i = 0; i < path_depth; i++){
		if(!current_node)goto open_error;
		vfs_node *next_node = vfs_finddir(current_node,path_array[i]);

		if(current_node != local_root){
			vfs_close(current_node);
		}

		current_node = next_node;
	}
	
	return current_node;

	open_error:
	kfree(new_path);
	return NULL;
}