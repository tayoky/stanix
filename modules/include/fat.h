#ifndef _FAT_H
#define _FAT_H

#include <kernel/vfs.h>
#include <stdint.h>

#define FAT12 1
#define FAT16 2
#define FAT32 3

#define FAT_EOF 0xFFFFFFFF

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

typedef struct fat {
    int fat_type;
	vfs_node *dev;
    uint16_t reserved_sectors;
    uint16_t sector_size;
    uint32_t sectors_per_fat;
    uint32_t cluster_size;
    off_t data_start;          //start of root/data section start
} fat;

//in memory inode
typedef struct fat_inode {
	fat_entry entry;
	uint32_t first_cluster;
    fat fat_info;
	//used for fat16/12 root
	int is_fat16_root;
	uint64_t start;
	uint16_t entries_count;
} fat_inode;


static vfs_node *fat_lookup(vfs_node *node,const char *name);
struct dirent *fat_readdir(vfs_node *node,uint64_t index);
ssize_t fat_read(vfs_node *node,void *buf,uint64_t offset,size_t count);

#endif