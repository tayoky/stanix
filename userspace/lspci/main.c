#include <sys/mount.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
//we are including kernel header in userspace program XD
#include <module/pci.h>

#define PCI_DIR "/sys/bus/pci/"

void help() {
	printf("lspci [OPTIOn]\n");
	printf("list pci devices\n");
}

const char *get_class_name(uint8_t class, uint8_t sub_class) {
	// this list comes from osdev.org
	switch (class) {
	case 0x0:
		switch (sub_class) {
		case 0x0:
			return "Non-VGA-Compatible Unclassified Device";
		case 0x1:
			return "VGA-Compatible Unclassified Device";
		default:
			return "Unclassified";
		}
	case 0x1:
		switch (sub_class) {
		case 0x0:
			return "SCSI Bus Controller";
		case 0x1:
			return "IDE Controller";
		case 0x2:
			return "Floppy Disk Controller";
		case 0x3:
			return "IPI Bus Controller";
		case 0x4:
			return "RAID Controller";
		case 0x5:
			return "ATA Controller";
		case 0x6:
			return "Serial ATA Controller";
		case 0x7:
			return "Serial Attached SCSI Controller";
		case 0x8:
			return "Non-Volatile Memory Controller";
		default:
			return "Mass Storage Controller";
		}
	case 0x2:
		switch (sub_class) {
		case 0x0:
			return "Ethernet Controller";
		case 0x1:
			return "Token Ring Controller";
		case 0x2:
			return "FDDI Controller";
		case 0x3:
			return "ATM Controller";
		case 0x4:
			return "ISDN Controller";
		case 0x5:
			return "WorldFip Controller";
		case 0x6:
			return "PICMG 2.14 Multi Computing Controller";
		case 0x7:
			return "Infiniband Controller";
		case 0x8:
			return "Fabric Controller";
		default:
			return "Network Controller";
		}
	case 0x3:
		switch (sub_class) {
		case 0x0:
			return "VGA Compatible Controller";
		case 0x1:
			return "XGA Controller";
		case 0x2:
			return "3D Controller (Not VGA-Compatible)";
		default:
			return "Display Controller";
		}
	case 0x6:
		switch (sub_class) {
		case 0x0:
			return "Host Bridge";
		case 0x1:
			return "ISA Bridge";
		case 0x2:
			return "EISA Bridge";
		case 0x3:
			return "MCA Bridge";
		case 0x4:
			return "PCI-to-PCI Bridge";
		case 0x5:
			return "PCMCIA Bridge";
		case 0x6:
			return "NuBus Bridge";
		case 0x7:
			return "CardBus Bridge";
		case 0x8:
			return "RACEway Bridge";
		case 0x9:
			return "PCI-to-PCI Bridge";
		case 0xa:
			return "InfiniBand-to-PCI Host Bridge";
		default:
			return "Bridge";
		}
	default:
		return "Unknow";
	}
}

uint16_t read_word(FILE *f, size_t offset) {
	fseek(f, offset, SEEK_SET);
	uint16_t ret;
	fread(&ret, sizeof(ret), 1, f);
	return ret;
}

void print_pci(const char *name) {
	char *full = malloc(strlen(PCI_DIR) + strlen(name) + 1);
	sprintf(full, PCI_DIR"%s", name);
	FILE *f = fopen(full, "r");
	free(full);
	if (!f) {
		perror(name);
		exit(1);
	}
	printf("%s ", name);
	uint16_t vendor = read_word(f, PCI_CONFIG_VENDOR_ID);
	uint16_t device = read_word(f, PCI_CONFIG_DEVICE_ID);
	printf("%04x:%04x ", vendor, device);
	uint16_t class = read_word(f, PCI_CONFIG_CLASS);
	uint8_t base_class = (class >> 8) & 0xff;
	uint8_t sub_class = class & 0xff;
	printf("%s[%hhu:%hhu]", get_class_name(base_class, sub_class), base_class, sub_class);
	putchar('\n');
	fclose(f);
}

int main(int argc, char **argv) {
	if (argc == 2 && !strcmp(argv[1], "--help")) {
		help();
		return 0;
	}
	DIR *pci = opendir(PCI_DIR);
	if (!pci) {
		perror(PCI_DIR);
		return 1;
	}

	for (;;) {
		struct dirent *ent = readdir(pci);
		if (!ent)break;
		if (ent->d_name[0] == '.')continue;
		print_pci(ent->d_name);
	}

	closedir(pci);
	return 0;
}