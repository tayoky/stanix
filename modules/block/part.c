#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/device.h>
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
} __attribute__((packed)) mbr_entry_t;

typedef struct mbr_struct {
	char bootstrap[440];
	uint32_t uuid;
	uint16_t reserved;
	mbr_entry_t entries[4];
	uint16_t signature;
} __attribute__((packed)) mbr_table_t;

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
} __attribute__((packed)) gpt_header_t;

typedef struct gpt_entry {
	struct gpt_guid type;
	struct gpt_guid guid;
	uint64_t lba_start;
	uint64_t lba_end;
	uint64_t attribute;
	char name[72];
} __attribute__((packed)) gpt_entry_t;

typedef struct part {
	device_t device;
	vfs_fd_t *dev;
	size_t size;
	off_t offset;
	struct part_info info;
} part_t;

static int part_ioctl(vfs_fd_t *fd,long req,void *arg){
	// expose partiton info to userspace
	part_t *partition = fd->private;
	switch (req) {
	case I_PART_GET_INFO:
		*(struct part_info *)arg = partition->info;
		return 0;
	default:
		return vfs_ioctl(partition->dev, req, arg);
	}
}

static ssize_t part_read(vfs_fd_t *fd,void *buf,off_t offset,size_t count){
	part_t *partition = fd->private;
	if((size_t)offset > partition->size){
		return 0;
	}
	if((size_t)offset + count > partition->size){
		count = partition->size - offset;
	}
	return vfs_read(partition->dev,buf,offset + partition->offset,count);
}

static ssize_t part_write(vfs_fd_t *fd,const void *buf,off_t offset,size_t count){
	part_t *partition = fd->private;
	if((size_t)offset > partition->size){
		return 0;
	}
	if((size_t)offset + count > partition->size){
		count = partition->size - offset;
	}
	return vfs_write(partition->dev,buf,offset + partition->offset,count);
}

static vfs_ops_t part_ops = {
	.read  = part_read,
	.write = part_write,
	.ioctl = part_ioctl,
};

static device_driver_t part_driver = {
	.name = "partitions",
};

static void swap_guid(struct gpt_guid *guid){
	guid->e4 = ((guid->e4 & 0xff) << 8) | ((guid->e4 >> 8) & 0xff);
}

static int create_part(vfs_fd_t *dev,const char *target,off_t offset,size_t size,int *count,struct part_info *info){
	kdebugf("find partition offset : %lx size : %ld\n",offset,size);
	char path[strlen(target) + 16];
	sprintf(path,"%s%d",dev->inode->name,(*count)++);

	part_t *p = kmalloc(sizeof(part_t));
	p->dev    = vfs_dup(dev);
	p->offset = offset;
	p->size   = size;
	p->info   = *info;
	p->device.type   = DEVICE_BLOCK;
	p->device.name   = strdup(path);
	p->device.driver = &part_driver;
	p->device.ops    = &part_ops;
	int ret = register_device((device_t*)dev);
	if (ret < 0) {
		kfree(p->device.name);
		kfree(p);
	}
	return ret;
}

int init_gpt(off_t offset,vfs_fd_t *dev,const char *target){
	gpt_header_t gpt;
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
		gpt_entry_t entry;
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

	vfs_fd_t *dev = vfs_open(source,O_RDONLY);
	if(!dev)return -ENOENT;

	mbr_table_t mbr;
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
	register_device_driver(&part_driver);
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
