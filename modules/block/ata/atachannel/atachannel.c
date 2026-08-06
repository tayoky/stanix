#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/bus.h>
#include <module/ata.h>

// TODO : maybee wipe out this and merge it into ata driver

// thanks Sasdallas for this
typedef struct ata_ident {
	uint16_t flags;           // If bit 15 is cleared, valid drive. If bit 7 is set to one, this is removable.
	uint16_t obsolete;        // Obsolete
	uint16_t specifics;       // 7.17.7.3 in specification
	uint16_t obsolete2[6];    // Obsolete
	uint16_t obsolete3;       // Obsolete
	char serial[20];          // Serial number
	uint16_t obsolete4[3];    // Obsolete
	char firmware[8];         // Firmware revision
	char model[40];           // Model number
	uint16_t rw_multiple;     // R/W multiple support (<=16 is SATA)
	uint16_t obsolete5;       // Obsolete
	uint32_t capabilities;    // Capabilities of the IDE device
	uint16_t obsolete6[2];    // Obsolete
	uint16_t field_validity;  // If 1, the values reported in _ - _ are valid
	uint16_t obsolete7[5];    // Obsolete
	uint16_t multi_sector;    // Multiple sector setting
	uint32_t sectors;         // Total addressable sectors
	uint16_t obsolete8[20];   // Technically these aren't obsolete, but they contain nothing really useful
	uint32_t command_sets;    // Command/feature sets
	uint16_t obsolete9[16];   // Contain nothing really useful
	uint64_t sectors_lba48;   // LBA48 maximum sectors, AND by 0000FFFFFFFFFFFF for validity
	uint16_t obsolete10[152]; // Contain nothing really useful
} __attribute__((packed)) __attribute__((aligned(16))) ata_ident_t;

static int ata_channel_probe(devnode_t *devnode) {
	ata_ident_t ident;
	ata_command_t identify = {
		.opcode = ATA_CMD_IDENTIFY,
		.lba = 0,
		.sectors_count = 0,
		.flags = ATA_CMD_SEND_LBA28,
		.buf = &ident,
	};
	int ret = ata_send_command(devnode, &identify);
	if (ret < 0) return ret;


	ata_device_t *device = kmalloc(sizeof(ata_device_t));
	if (!device) return -ENOMEM;
	memset(device, 0, sizeof(ata_device_t));
	device->signature = ata_get_signature(devnode);

	device->sectors_count = ident.command_sets & (1 << 26) ? ident.sectors_lba48 : ident.sectors;
	for (size_t i = 0; i < sizeof(ident.model); i += 2) {
		device->model[i + 1] = ident.model[i];
		device->model[i]     = ident.model[i + 1];
	}
	for (size_t i = sizeof(device->model) - 1; i > 0 && device->model[i] == ' '; i--) {
		device->model[i] = '\0';
	}
	device->command_sets = ident.command_sets;

	kdebugf("model : %s command sets : %x support LBA48 : %s max LBA : %ld\n", device->model, device->command_sets, device->command_sets & (1 << 26) ? "true" : "false", device->sectors_count);

	bus_attach_child(devnode, &device->devnode, NULL, UNIT_NOUNIT);
	return 0;
}

static void ata_channel_detach(devnode_t *devnode) {
	(void)devnode;
	// nothing to do
}

static int ata_channel_send_ata_command(devnode_t *channel, devnode_t *device, ata_command_t *command) {
	(void)device;
	// pass the ata command to parent
	return ata_send_command(channel, command);
}

static ata_driver_t ata_channel_driver = {
	.driver = {
		.name = "ATA channel",
		.device_name = "ata_channel",
		.probe = ata_channel_probe,
		.detach = ata_channel_detach,
	},
	.send_ata_command = ata_channel_send_ata_command,
};

int ata_channel_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return driver_register(&ata_channel_driver.driver);
}

int ata_channel_fini(void) {
	return driver_unregister(&ata_channel_driver.driver);
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = ata_channel_init,
	.fini        = ata_channel_fini,
	.author      = "tayoky",
	.name        = "atachannel",
	.description = "ATA channel driver",
	.license     = "GPL 3",
};
