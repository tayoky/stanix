#include <kernel/module.h>
#include <kernel/vfs.h>
#include <kernel/print.h>
#include <stdint.h>
#include <errno.h>

//module for partions (mbr/gpt)

typedef struct mbr_entry_struct {
	uint8_t attribute;
	char chs_start[3];
	uint8_t type;
	char chs_end[3];
	uint32_t lba_start;
	uint32_t sectors_count;
} mbr_entry;

typedef struct mbr_struct {
	char bootstrap[440];
	uint32_t uuid;
	uint16_t reserved;
	mbr_entry entries[4];
	uint16_t signature;
} __attribute__((packed)) mbr_table;

int part_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)data;
	(void)flags;

	kdebugf("mount %s to %s\n",source,target);

	vfs_node *dev = vfs_open(source,VFS_READONLY);
	if(!dev)return -ENOENT;

	mbr_table mbr;
	vfs_read(dev,&mbr,0,sizeof(mbr));

	return 0;
}

vfs_filesystem part_fs = {
	.mount = part_mount,
	.name = "part",
};

int part_init(int argc,char **argv){
	vfs_register_fs(&part_fs);
	return 0;
}

int part_fini(){
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = part_init,
	.fini = part_init,
	.author = "tayoky",
	.name = "test",
	.description = "just a simple module to test",
	.license = "GPL 3",
};