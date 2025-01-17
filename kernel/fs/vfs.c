#include "vfs.h"
#include "kernel.h"
#include "print.h"
#include "kheap.h"
#include "string.h"

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
