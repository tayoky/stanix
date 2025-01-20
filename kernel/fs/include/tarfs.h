#ifndef TARFS_H
#define TARFS_H
#include <stdint.h>

typedef struct ustar_header_struct{
	char name[100];
	char file_mode[8];
	char owner[8];
	char group[8];
	char file_size[12];
	char last_modif[12];
	uint64_t checksum;
	char type;
	char linked_file[100];
	char ustar[6];
	char owner_name[32];
	char group_name[32];
	uint64_t major_number;
	uint64_t minor_number;
	char prefix[155];
}__attribute__((packed)) ustar_header;

#define USTAR_REGTYPE  '0'        // Regular file (preferred code)
#define USTAR_AREGTYPE '\0'       // Regular file (alternate code)
#define USTAR_LNKTYPE  '1'        // Hard link
#define USTAR_SYMTYPE  '2'        // Symbolic link (hard if not supported)
#define USTAR_CHRTYPE  '3'        // Character special
#define USTAR_BLKTYPE  '4'        // Block special
#define USTAR_DIRTYPE  '5'        // Directory
#define USTAR_FIFOTYPE '6'        // Named pipe
#define USTAR_CONTTYPE '7'        // Contiguous file

void mount_initrd(void);

#endif