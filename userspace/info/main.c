#include <module/part.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/block.h>
#include <sys/device.h>
#include <libtrm/trm.h>
#include <libinput.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

void help(void) {
	puts("info DEVICE");
	puts("show information about a device");
}

const char *trm_plane_type(uint32_t type) {
	switch (type) {
	case TRM_PLANE_PRIMARY:
		return "primary plane";
	case TRM_PLANE_CURSOR:
		return "cursor plane";
	case TRM_PLANE_OVERLAY:
		return "overlay plane";
	default:
		return "unknow plane type";
	}
}

const char *byte_amount(size_t amount) {
	static char *suffix[] = {
		"b",
		"Kb",
		"Mb",
		"Gb",
		"Tb",
		"Pb",
		NULL,
	};
	int i = 0;
	while (amount >= 1024 && suffix[i + 1]) {
		i++;
		amount /= 1024;
	}
	static char buf[32];
	sprintf(buf, "%zu%s", amount, suffix[i]);
	return buf;
}

static const char *gpt_guid2str(gpt_guid_t *guid) {
	static char buf[256];
	int ptr = sprintf(buf, "%08x-%04hx-%04hx-%04hx-", guid->e1, guid->e2, guid->e3, guid->e4);
	for (int i = 0; i < 6; i++) {
		ptr += sprintf(buf + ptr, "%02hhx", guid->e5[i]);
	}
	return buf;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "not enought argument\n");
		return 1;
	}
	if (!strcmp(argv[1], "--help")) {
		help();
		return 0;
	}
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}
	printf("%s :\n", argv[1]);
	struct stat st;
	if (fstat(fd, &st) >= 0) {
		printf("device     : %u, %u\n", major(st.st_rdev), minor(st.st_rdev));
	}

	// try to get (legacy) partition info
	part_info_t part_info;
	if (ioctl(fd, PART_GET_INFO, &part_info) >= 0) {
		switch (part_info.type) {
		case PART_TYPE_MBR:
			printf("part type  : MBR\n");
			printf("part uuid  : %08x\n", part_info.mbr.disk_uuid);
			printf("part type  : %02hhx(MBR)\n", part_info.mbr.type);
			break;
		case PART_TYPE_GPT:
			printf("part type  : GPT\n");
			printf("disk uuid  : %s\n", gpt_guid2str(&part_info.gpt.disk_uuid));
			printf("part uuid  : %s\n", gpt_guid2str(&part_info.gpt.part_uuid));
			printf("part type  : %s\n", gpt_guid2str(&part_info.gpt.type));
			break;
		default:
			printf("part type  : unknown\n");
			break;
		}
	}

	// try to get device info
	device_info_t device_info = { 0 };
	if (ioctl(fd, DEVICE_GET_INFO, &device_info) >= 0) {
		if (device_info.product[0])  printf("product    : %s\n", device_info.product);
		if (device_info.vendor[0])   printf("vendor     : %s\n", device_info.vendor);
		if (device_info.firmware[0]) printf("firmware   : %s\n", device_info.firmware);
		if (device_info.serial[0])   printf("serial     : %s\n", device_info.serial);
	}

	// try to get block info
	block_disk_info_t disk_info;
	if (ioctl(fd, BLOCK_GET_DISK_INFO, &disk_info) >= 0) {
		if (disk_info.uuid[0]) printf("disk uuid  : %s\n", disk_info.uuid);
		if (disk_info.partition_table_type[0]) printf("part type  : %s\n", disk_info.partition_table_type);
		printf("block size : %s\n", byte_amount(disk_info.logicial_block_size));
		printf("blocks     : %zu\n", disk_info.blocks_count);
		printf("size       : %s\n", byte_amount(disk_info.logicial_block_size * disk_info.blocks_count));
	}

	// try to get (modern) partition info
	block_part_info_t block_part_info;
	if (ioctl(fd, BLOCK_GET_PART_INFO, &block_part_info) >= 0) {
		if (block_part_info.uuid[0]) printf("part uuid  : %s\n", block_part_info.uuid);
		printf("offset     : %zd\n", block_part_info.offset);
		printf("size       : %s\n", byte_amount(block_part_info.size));
	}


	// try to get input info
	struct input_info input_info;
	if (libinput_get_info(fd, &input_info) >= 0) {
		printf("class      : %s\n", libinput_class_string(input_info.if_class));
		printf("subclass   : %s\n", libinput_subclass_string(input_info.if_class, input_info.if_subclass));
	}
	char layout[INPUT_LAYOUT_SIZE];
	if (libinput_get_layout(fd, layout) >= 0 && layout[0]) {
		printf("layout     : %s\n", layout);
	}

	// try to print trm info
	trm_card_t *card = trm_get_resources(fd);
	if (card) {
		printf("card       : %s\n", card->name);
		printf("driver     : %s\n", card->driver);
		printf("vram       : %s\n", byte_amount(card->vram_size));
		printf("planes     : %lu\n", card->planes_count);
		printf("crtcs      : %lu\n", card->crtcs_count);
		printf("connectors : %lu\n", card->connectors_count);
		for (size_t i=0; i < card->planes_count; i++) {
			trm_plane_t *plane = &card->planes[i];
			printf("plane(%u) : %s\n", plane->id, trm_plane_type(plane->type));
		}
	}
	return 0;
}
