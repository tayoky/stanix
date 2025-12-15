#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/vfs.h>
#include <module/part.h>
#include <stdint.h>
#include <errno.h>

//module for partions (mbr/gpt)

#define GPT_ID 0xEE 

typedef struct mbr_entry_struct {
	uint8_t attribute;
	char chs_start[3];
	uint8_t type;
	char chs_end[3];
	uint32_t lba_start;
	uint32_t sectors_count;
} __attribute__((packed)) mbr_entry;

typedef struct mbr_struct {
	char bootstrap[440];
	uint32_t uuid;
	uint16_t reserved;
	mbr_entry entries[4];
	uint16_t signature;
} __attribute__((packed)) mbr_table;

typedef struct gpt_struct {
	char signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t checksum;
	uint32_t reserved;
	uint64_t lba_header;
	uint64_t lba_alt_header;
	uint64_t lba_start;
	uint64_t lba_end;
	struct gpt_guid guid;
	uint64_t lba_guid;
	uint32_t part_count;
	uint32_t part_ent_size;
	uint32_t checksum_part;
} __attribute__((packed)) gpt_header;

typedef struct gpt_entry {
	struct gpt_guid type;
	struct gpt_guid guid;
	uint64_t lba_start;
	uint64_t lba_end;
	uint64_t attribute;
	char name[72];
} __attribute__((packed)) gpt_entry;

typedef struct part {
	vfs_node *dev;
	size_t size;
	off_t offset;
	struct part_info info;
} part;

static int part_ioctl(vfs_node *node,long req,void *arg){
	//expose partiton info to userspace
	part *partition = node->private_inode;
	switch(req){
	case I_PART_GET_INFO:
		*(struct part_info *)arg = partition->info;
		return 0;
	default:
		return -EINVAL;
	}
}

static ssize_t part_read(vfs_node *node,void *buf,uint64_t offset,size_t count){
	part *partition = node->private_inode;
	if(offset > partition->size){
		return 0;
	}
	if(offset + count > partition->size){
		count = partition->size - offset;
	}
	return vfs_read(partition->dev,buf,offset + partition->offset,count);
}

static ssize_t part_write(vfs_node *node,void *buf,uint64_t offset,size_t count){
	part *partition = node->private_inode;
	if(offset > partition->size){
		return 0;
	}
	if(offset + count > partition->size){
		count = partition->size - offset;
	}
	return vfs_write(partition->dev,buf,offset + partition->offset,count);
}

void swap_guid(struct gpt_guid *guid){
	guid->e4 = ((guid->e4 & 0xff) << 8) | ((guid->e4 >> 8) & 0xff);
}

static void create_part(vfs_node *dev,const char *target,off_t offset,size_t size,int *count,struct part_info *info){
	kdebugf("find partition offset : %lx size : %ld\n",offset,size);
	char path[strlen(target) + 16];
	sprintf(path,"%s%d",target,(*count)++);

	part *p = kmalloc(sizeof(part));
	p->dev    = vfs_dup(dev);
	p->offset = offset;
	p->size   = size;
	p->info   = *info;
	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->private_inode = p;
	node->flags = VFS_DEV | VFS_BLOCK;
	node->read  = part_read;
	node->write = part_write;
	node->ioctl = part_ioctl;
	vfs_create(path,0700,VFS_FILE);
	vfs_mount(path,node);
}

int init_gpt(off_t offset,vfs_node *dev,const char *target){
	gpt_header gpt;
	vfs_read(dev,&gpt,offset,sizeof(gpt));
	if(memcmp(gpt.signature,"EFI PART",8)){
		vfs_close(dev);
		return -EIO; //what error to return ?
	}
	//TODO : check the checksum

	off_t off = offset + 512;
	int counter = 0;
	struct part_info info = {
		.type = PART_TYPE_GPT,
	};
	memcpy(&info.gpt.disk_uuid,&gpt.guid,sizeof(gpt.guid));
	swap_guid(&info.gpt.disk_uuid);
	for (size_t i = 0; i < gpt.part_count; i++,off += gpt.part_ent_size){
		gpt_entry entry;
		vfs_read(dev,&entry,off,sizeof(entry));

		//ignore empty partitions
		struct gpt_guid zero;
		memset(&zero,0,sizeof(zero));
		if(!memcmp(&entry.guid,&zero,sizeof(struct gpt_guid)))continue;

		memcpy(&info.gpt.part_uuid,&entry.guid,sizeof(entry.guid));
		memcpy(&info.gpt.type     ,&entry.type,sizeof(entry.type));
		swap_guid(&info.gpt.part_uuid);
		swap_guid(&info.gpt.type);

		create_part(dev,target,entry.lba_start * 512,(entry.lba_end - entry.lba_start)*512,&counter,&info);
	}
	
	return 0;
}

int part_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)data;
	(void)flags;

	kdebugf("mount %s to %s\n",source,target);

	vfs_node *dev = vfs_open(source,VFS_READONLY);
	if(!dev)return -ENOENT;

	mbr_table mbr;
	vfs_read(dev,&mbr,0,sizeof(mbr));

	//check for gpt first
	for (size_t i = 0; i < 4; i++){
		if(mbr.entries[i].type == GPT_ID){
			//gpt !
			return init_gpt((off_t)mbr.entries[i].lba_start * 512,dev,target);
		}
	}

	int counter = 0;
	struct part_info info = {
		.type = PART_TYPE_MBR,
		.mbr = {
			.disk_uuid = mbr.uuid,
		},
	};
	for (size_t i = 0; i < 4; i++){
		if(!mbr.entries[i].sectors_count)continue;
		info.type = mbr.entries[i].type;
		create_part(dev,target,mbr.entries[i].lba_start * 512,mbr.entries[i].lba_start * 512,&counter,&info);
	}
	vfs_close(dev);

	return 0;
}

vfs_filesystem part_fs = {
	.mount = part_mount,
	.name = "part",
};

int part_init(int argc,char **argv){
	(void)argc;
	(void)argv;
	vfs_register_fs(&part_fs);
	return 0;
}

int part_fini(){
	//TODO : unregister
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = part_init,
	.fini = part_fini,
	.author = "tayoky",
	.name = "part",
	.description = "partiton system for GPT/MBR",
	.license = "GPL 3",
};