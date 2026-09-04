#ifndef MODULE_RETROFS
#define MODULE_RETROFS

#include <kernel/cache.h>
#include <kernel/vfs.h>
#include <stdint.h>

typedef struct retrofs_description_block {
	uint8_t identifier[8];
	uint64_t root_directory;
	uint64_t free_space_map_start;
	uint64_t free_space_map_length;
	uint64_t free_space_map_checksum;
	uint64_t sequence_counter;
	uint64_t creation_time;
	char reserved[456];
} __attribute__((packed)) retrofs_description_block_t;

#define RETROFS_IDENTIFIER "RetroFS1"

#define RETROFS_FS_BITS_PER_SECTOR (512 / 8)
typedef struct retrofs_free_space_map_entry {
	uint64_t bits[RETROFS_FS_BITS_PER_SECTOR];
} __attribute__((packed)) retrofs_free_space_map_entry_t;

#define RETROFS_DEFAULT_DIR_SIZE 64

typedef struct retrofs_directory_start {
	uint32_t flags;
	char title[128];
	uint64_t parent;
	uint64_t sectors;
	uint64_t continuation;
	char reserved[];
} __attribute__((packed)) retrofs_directory_start_t;

typedef struct retrofs_directory_entry {
	uint32_t flags;
	char filename[128];
	uint64_t start_sector;
	uint64_t length;
	uint64_t sectors_count;
	uint64_t creation_time;
	uint64_t modification_time;
	uint64_t sequence_counter;
	char reserved[];
} __attribute__((packed)) retrofs_directory_entry_t;

#define RETRO_FS_FLAG_DIRECTORY 0x01
#define RETRO_FS_FLAG_LOCKED    0x02
#define RETRO_FS_FLAG_DIR_START 0x04

typedef struct retrofs_superblock {
	vfs_superblock_t superblock;
} retrofs_superblock_t;

typedef struct retrofs_inode {
	vfs_node_t vnode;
	cache_t cache;
	off_t entry_offset;
} retrofs_inode_t;

#endif
