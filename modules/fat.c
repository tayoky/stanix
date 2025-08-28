#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/vfs.h>
#include <stdint.h>
#include <errno.h>

#define FAT12 1
#define FAT16 2
#define FAT32 3

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

//also for fat12
typedef struct fat16_bpb {
	uint8_t drive_num;
	uint8_t reserved;
	uint8_t boot_signature;
	uint32_t volume_id;
	char volume_label[11];
	char fs_type[8];
	uint8_t reserved1[448];
	uint16_t signature;
} __attribute__((packed)) fat16_bpb;

typedef struct fat32_bpb {
	uint32_t sectors_per_fat32;
	uint16_t ext_flags;
	uint16_t version;      //must be 0
	uint32_t root_cluster; //first cluster of root
	uint16_t fs_info;
	uint16_t bk_boot_sector;
	char reserved[12];
	uint8_t drive_num;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	char volume_label[11];
	char fs_type[8];
	char reserved2[420];
	uint16_t signature;
} __attribute__((packed)) fat32_bpb;

typedef struct fat_bpb {
	char jmp_boot[3];
	char oem_name[8];
	uint16_t byte_per_sector;
	uint8_t sector_per_cluster;
	uint16_t reserved_sectors; //count
	uint8_t fat_count;
	uint16_t root_entires_count; //only fat12/16
	uint16_t sectors_count16;    //16 bits version of total sectors count (must be 0 for fat32)
	uint8_t media;
	uint16_t sectors_per_fat16;  //only fat12/16
	uint16_t sectors_per_track;
	uint16_t head_count;
	uint32_t hidden_sectors;
	uint32_t sectors_count32;    //32 bits version of total sectors count (must be not 0 if 16bits version is 0 or fat32)
	union {
		fat32_bpb fat32;
		fat16_bpb fat16;
	} extended;
} __attribute__((packed)) fat_bpb;

typedef struct fat_entry {
	char name[11];
	uint8_t attribute;
	uint8_t reserved;
	uint8_t creation_time_tenth;
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t access_date;
	uint16_t cluster_higher; //high 16 bits of first cluser MUST BE 0 on fat 12/16
	uint16_t write_time;
	uint16_t write_date;
	uint16_t cluster_lower;  //low 16 bits of first cluser
	uint32_t file_size;
} __attribute__((packed)) fat_entry;

//in memory inode
typedef struct fat_inode {
	vfs_node *dev;
	fat_entry entry;
	uint32_t first_cluster;
	int fat_type;
	//used for fat16/12 root
	int is_fat16_root;
	uint64_t start;
	uint16_t entries_count;
} fat_inode;

static vfs_node *fat_entry2node(fat_entry *entry,vfs_node *dev,int fat_type){
	fat_inode *inode = kmalloc(sizeof(fat_inode));
	memset(inode,0,sizeof(fat_inode));
	inode->dev = dev;
	inode->fat_type = fat_type;
	inode->entry = *entry;
	inode->first_cluster = (entry->cluster_higher << 8) | entry->cluster_lower;

	vfs_node *node = kmalloc(sizeof(vfs_node));
	memset(node,0,sizeof(vfs_node));
	node->private_inode = inode;
	if(entry->attribute & ATTR_DIRECTORY){
		node->flags |= VFS_DIR;
	} else {
		node->flags |= VFS_FILE;
	}
	return node;
}



vfs_node *fat_lookup(vfs_node *node,const char *name){
	fat_inode *inode = node->private_inode;
	return NULL;
}

struct dirent *fat_readdir(vfs_node *node,uint64_t index){
	fat_inode *inode = node->private_inode;
	uint64_t offset = inode->start;
	uint32_t remaning = inode->entries_count;
	while (index > 0){
		if(!remaning)return NULL;
		//skip everything with VOLUME_ID attr or free
		for(;;){
			fat_entry entry;
			vfs_read(inode->dev,&entry,offset,sizeof(entry));
			if(entry.name[0] == 0x00){
				//everything is free after that
				//we hit last
				return NULL;
			}
			if(!(entry.attribute & ATTR_VOLUME_ID) && (entry.name[0] != (char)0xe5))break;
			offset += sizeof(fat_entry);
			remaning--;
			if(!remaning)return NULL;
		}
		index--;
		offset += sizeof(fat_entry);
		remaning--;
	}
	if(!remaning)return NULL;

	//skip everything with VOLUME_ID attr
	//TODO : handle long name
	for(;;){
		fat_entry entry;
		vfs_read(inode->dev,&entry,offset,sizeof(entry));
		if(entry.name[0] == 0x00){
			//everything is free after that
			//we hit last
			return NULL;
		}
		if(!(entry.attribute & ATTR_VOLUME_ID) && (entry.name[0] != (char)0xe5))break;
		offset += sizeof(fat_entry);
		remaning--;
		if(!remaning)return NULL;
	}

	//now parse the entry
	fat_entry entry;
	vfs_read(inode->dev,&entry,offset,sizeof(entry));
	
	struct dirent *ent = kmalloc(sizeof(struct dirent));
	size_t j=0;
	for(int i=0; i<8; i++){
		if(entry.name[i] == ' ')break;
		ent->d_name[j++] = entry.name[i];
	}
	//don't add . for directories without extention
	if(!((entry.attribute & ATTR_DIRECTORY) && entry.name[8] == ' ')){
		ent->d_name[j++] = '.';
	}
	for(int i=8; i<11; i++){
		if(entry.name[i] == ' ')break;
		ent->d_name[j++] = entry.name[i];
	}
	ent->d_name[j] = '\0';
	return ent;
}

int fat_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)flags;
	(void)data;
	vfs_node *dev = vfs_open(source,VFS_READONLY);
	if(!dev)return -ENOENT;

	fat_bpb bpb;
	if(vfs_read(dev,&bpb,0,sizeof(bpb)) != sizeof(bpb)){
		vfs_close(dev);
		return -EIO; //not sure this is the good error code
	}
	kdebugf("%ld\n",offsetof(fat_bpb,extended.fat32.signature));
	if(bpb.extended.fat32.signature != 0xaa55){
		kdebugf("invalid signature\n");
		vfs_close(dev);
		return -EFTYPE;
	}

	kdebugf("oem is '%s'\n",bpb.oem_name);

	if(bpb.byte_per_sector < 512 || bpb.sector_per_cluster == 0){
		kdebugf("invalid byte per sectors / sectors per cluster\n");
		vfs_close(dev);
		return -EFTYPE;
	}
	
	uint16_t root_sectors = ((bpb.root_entires_count * 32) + bpb.byte_per_sector - 1) / bpb.byte_per_sector;
	uint32_t sectors_per_fat = bpb.sectors_per_fat16 ? bpb.sectors_per_fat16 : bpb.extended.fat32.sectors_per_fat32;
	uint32_t sectors_count   = bpb.sectors_count16   ? bpb.sectors_count16   : bpb.sectors_count32;
	uint32_t data_sectors    = sectors_count - (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat + root_sectors);
	uint32_t data_clusters   = data_sectors / bpb.sector_per_cluster;

	int fat_type;
	if(data_clusters < 4085){
		fat_type = FAT12;
		kdebugf("fat12\n");
	} else if(data_clusters < 65525){
		fat_type = FAT16;
		kdebugf("fat16\n");
	} else {
		fat_type = FAT32;
		kdebugf("fat32\n");
	}

	if(fat_type == FAT32){
		if(bpb.extended.fat32.version != 0){
			vfs_close(dev);
			return -EFTYPE;
		}
	} else {
		//TODO : fat12/16 check
	}

	vfs_node *local_root;
	if(fat_type == FAT32){
		//build a fake entry for root
		fat_entry root_entry;
		memset(&root_entry,0,sizeof(root_entry));
		root_entry.attribute = ATTR_DIRECTORY;
		root_entry.cluster_lower  =  bpb.extended.fat32.root_cluster & 0xf;
		root_entry.cluster_higher =  (bpb.extended.fat32.root_cluster >> 8) & 0xf;
		//how do we get size ?
		local_root = fat_entry2node(&root_entry,dev,fat_type);
	} else {
		//root of fat12/16 is really stupid
		fat_inode *root = kmalloc(sizeof(fat_inode));
		root->dev = dev;
		root->entries_count = bpb.root_entires_count;
		root->fat_type = fat_type;
		root->start = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector;
		root->is_fat16_root = 1;

		local_root = kmalloc(sizeof(vfs_node));
		memset(local_root,0,sizeof(vfs_node));
		local_root->private_inode = root;
		local_root->flags = VFS_DIR;
		local_root->readdir = fat_readdir;
	}

	

	int ret = vfs_mount(target,local_root);
	if(ret < 0){
		kfree(local_root->private_inode);
		kfree(local_root);
		vfs_close(dev);
		return ret;
	}

	return 0;
}

vfs_filesystem fat_fs = {
	.mount = fat_mount,
	.name = "fat",
};

int part_init(int argc,char **argv){
	(void)argc;
	(void)argv;
	vfs_register_fs(&fat_fs);
	return 0;
}

int part_fini(){
	//TODO : make sure no partition is mounted
	vfs_unregister_fs(&fat_fs);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = part_init,
	.fini = part_fini,
	.author = "tayoky",
	.name = "fat",
	.description = "fat12/16/32 drivers",
	.license = "GPL 3",
};