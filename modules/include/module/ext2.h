#ifndef _MODULE_EXT2
#define _MODULE_EXT2

typedef struct ext2_superblock {
	uint32_t s_inodes_count;
	uint32_t s_blocks_count;
	uint32_t s_r_blocks_count;
	uint32_t s_free_blocks_count;
	uint32_t s_free_inodes_count;
	uint32_t s_fist_data_block;
	uint32_t s_log_block_size;
	uint32_t s_log_frag_size;
	uint32_t s_blocks_per_group;
	uint32_t s_frags_per_group;
	uint32_t s_inodes_per_group;
	uint32_t s_mtime;
	uint32_t s_wtime;
	uint16_t s_mnt_count;
	uint16_t s_max_mnt_count;
	uint16_t s_magic;
	uint16_t s_state;
	uint16_t s_errors;
	uint16_t s_minor_rev_level;
	uint32_t s_lastcheck;
	uint32_t s_checkinterval;
	uint32_t s_creator_os;
	uint32_t s_rev_level;
	uint16_t s_def_resuid;
	uint16_t s_def_resgid;

	// EXT2_DYNAMIC_REV specific
	uint32_t s_first_ino;
	uint16_t s_inode_size;
	uint16_t s_block_group_nr;
	uint32_t s_feature_compat;
	uint32_t s_feature_incompat;
	uint32_t s_feature_ro_compat;
	uint64_t s_uuid[2];
	uint8_t  s_volume_name[16];
	uint8_t  s_last_mounted[64];
	uint32_t s_algo_bitmap;
	uint8_t  s_prealloc_blocks;
	uint8_t  s_prealloc_dir_blocks;
	uint16_t alignement1;
	uint64_t s_journal_uuid[2];
	uint32_t s_journal_inum;
	uint32_t s_journal_dev;
	uint32_t s_last_orphan;
	uint32_t s_hash_seed[4];
	uint8_t  s_def_hash_version;
	uint8_t  alignement2[3];
	uint32_t s_default_mount_options;
	uint32_t s_first_meta_bg;
} __attribute__((packed)) ext2_superblock_t;

// s_state values
#define EXT2_VALID_FS 1 // unmounted cleanly
#define EXT2_ERROR_FS 2 // errors detected

// s_errors values
#define EXT2_ERRORS_CONTINUE 1 // continue as if nothing happened
#define EXT2_ERRORS_RO       2 // remount read-only
#define EXT2_ERRORS_PANIC    3 // cause a kernel panic

// s_creator_os values
#define EXT2_OS_LINUX 0 // Linux
#define EXT2_OS_HURD  1 // GNU/Hurd
#define EXT2_MASIX    2 // Masix
#define EXT2_FREEBSD  3 // FreeBSD
#define EXT2_LITES    4 // Lites

// s_rev_level values
#define EXT2_GOOD_OLD_REV 0 // Revision 0
#define EXT2_DYNAMIC_REV  1 // Revision 1

// s_first_ino fallback
#define EXT2_GOOD_OLD_FIRST_INO 11

// s_inode_size fallback
#define EXT2_GOOD_OLD_INODE_SIZE 128

// s_feature_compat values
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC  0x0001 // block preallocation for new directories
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL   0x0004 // an ext3 journal exist
#define EXT2_FEATURE_COMPAT_EXT_ATTR      0x0008 // extended inodes attributes are present
#define EXT2_FEATURE_COMPAT_RESIZE_INO    0x0010 // non standard inode size used
#define EXT2_FEATURE_COMPAT_DIR_INDEX     0x0020 // directory indexing via HTree


// s_feature_incompat values
#define EXT2_FEATURE_INCOMPAT_COMPRESSION 0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE    0x0002
#define EXT2_FEATURE_INCOMPAT_RECOVER     0x0004
#define EXT2_FEATURE_INCOMPAT_JOURNAL_DEV 0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG     0x0010

// s_feature_ro values
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004

// s_algo_bitmap values
#define EXT2_LZV1_ALG   0
#define EXT2_LZRW3A_ALG 1
#define EXT2_GZIP_ALG   2
#define EXT2_BZIP2_ALG  3
#define EXT2_LZO_ALG    4

typedef struct ext2_block_group_desc {
	uint32_t bg_block_bitmap;
	uint32_t bg_inode_bitmap;
	uint32_t bg_inode_table;
	uint16_t bg_free_blocks_count;
	uint16_t bg_free_inodes_count;
	uint16_t bg_used_dirs_count;
	uint16_t bg_pad;
	uint8_t bg_reserved[12];
} __attribute__((packed)) ext2_block_group_desc_t;

typedef struct ext2_hurd_osd2 {
	uint8_t h_i_frag;
	uint8_t h_i_fsize;
	uint16_t h_i_mode_high;
	uint16_t h_i_uid_high;
	uint16_t h_i_gid_high;
	uint32_t h_i_author;
} __attribute__((packed)) ext2_hurd_osd2_t;

typedef struct ext2_linux_osd2 {
	uint8_t l_i_frag;
	uint8_t l_i_fsize;
	uint16_t reserved1;
	uint16_t l_i_uid_high;
	uint16_t l_i_gid_high;
	uint32_t reserved2;
}__attribute__((packed)) ext2_linux_osd2_t;

typedef struct ext2_masix_osd2 {
	uint8_t m_i_frag;
	uint8_t m_i_fsize;
	uint8_t reserved[10];
}__attribute__((packed)) ext2_masix_osd2_t;

typedef ext2_inode {
	uint16_t i_mode;
	uint16_t i_uid;
	uint32_t i_size;
	uint32_t i_atime;
	uint32_t i_ctime;
	uint32_t i_mtime;
	uint32_t i_dtime;
	uint16_t i_gid;
	uint16_t i_links_count;
	uint32_t i_blocks;
	uint32_t i_flags;
	uint32_t i_osd1;
	uint32_t i_blocks[15];
	uint32_t i_generation;
	uint32_t i_file_acl;
	uint32_t i_dir_acl;
	uint32_t i_faddr;
	union {
		ext2_hurd_osd2_t hurd;
		ext2_linux_osd2_t linux;
		ext2_masix_osd2_t masix;
	} i_osd2;
} __attribute__((packed)) ext2_inode_t;

// reserved inode numbers
#define EXT2_BAD_INO         1 // bad blocks inode
#define EXT2_ROOT_INO        2 // root directory inode
#define EXT2_ACL_IDX_INO     3 // ACL index inode
#define EXT2_ACL_DATA_INO    4 // ACL data inode
#define EXT2_BOOT_LOADER_INO 5 // bootloader inode
#define EXT2_UNDEL_DIR_INO   6 // undelete directory inode

// i_flags values
#define EXT2_SECRM_FL        0x00000001 // secure deletion
#define EXT2_UNRM_FL         0x00000002 // record for undelete
#define EXT2_COMPR_FL        0x00000004 // compressed file
#define EXT2_SYNC_FL         0x00000008 // synchronous update
#define EXT2_IMMUTABLE_FL    0x00000010 // immutable file
#define EXT2_APPEND_FL       0x00000020 // append only
#define EXT2_NODUMP_FL       0x00000040 // do not dump/delete file
#define EXT2_NOATIME_FL      0x00000080 // do not update time atime
#define EXT2_DIRTY_FL        0x00000100 // dirty
#define EXT2_COMPRBLK_FL     0x00000200 // compressed blocks
#define EXT2_NOCOMPR_FL      0x00000400 // access raw compressed data
#define EXT2_ECOMPR_FL       0x00000800 // compression error
#define EXT2_BTREE_FL        0x00001000 // b-tree format directory
#define EXT2_INDEX_FL        0x00001000 // hash indexed directory
#define EXT2_IMAGIC_FL       0x00002000 // AFS directory
#define EXT2_JOURNAL_DATA_FL 0x00004000 // journal file data
#define EXT2_RESERVED_FL     0x80000000 // reserved for ext2 library

typedef struct ext2_linked_dentry {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t  name_len;
	uint8_t  file_type;
	uint8_t  name[];
} __attribute__((packed)) ext2_linked_dentry_t;

// file_type values
#define EXT2_FT_UNKNOW   0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

#endif
