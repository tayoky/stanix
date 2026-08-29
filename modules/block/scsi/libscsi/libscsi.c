#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/bus.h>
#include <module/scsi.h>

static slab_cache_t scsi_commands_slab;

static int scsi_submit_command(ioreq_t *ioreq) {
	scsi_command_t *scsi_command = container_of(ioreq, scsi_command_t, ioreq);
	scsi_driver_t *scsi_driver = container_of(scsi_command->device->bus->driver, scsi_driver_t, driver);
	kdebugf("send ");
	scsi_print_command(scsi_command);
	kprintf("\n");
	return scsi_driver->submit_scsi_command(scsi_command->device->bus, scsi_command->device, scsi_command);
}

static void scsi_free_command(ioreq_t *ioreq) {
	scsi_command_t *scsi_command = container_of(ioreq, scsi_command_t, ioreq);
	iobuf_destroy(&scsi_command->iobuf);
	slab_free(scsi_command);
}

static ioreq_ops_t scsi_command_ops = {
	.submit  = scsi_submit_command,
	.cleanup = scsi_free_command,
};

scsi_command_t *scsi_create_command(scsi_device_t *device, void *data, size_t size) {
	scsi_driver_t *scsi_driver = container_of(device->bus->driver, scsi_driver_t, driver);
	if (!scsi_driver->submit_scsi_command) return NULL;
	scsi_command_t *command = slab_alloc(&scsi_commands_slab);
	if (!command) return NULL;
	memset(command, 0, sizeof(scsi_command_t));
	command->device = device;
	command->ioreq.ops = &scsi_command_ops;
	if (data) {
		kassert(size <= sizeof(command->data));
		memcpy(&command->data, data, size);
	}
	command->data_length = size;
	return command;
}

scsi_command_t *scsi_create_read_command(scsi_device_t *device, size_t lba, size_t transfer_length, uint8_t flags) {
	scsi_command_t *command = NULL;
	if (lba > 0xffffffff) {
		// TODO : use READ(16)
		return NULL;
	} else if (transfer_length > 0xffff) {
		// use READ(12)
		scsi_read12_t cmd = {
			.opcode = SCSI_READ12_OPCODE,
			.flags  = flags,
			.lba = scsi_uint32_to_data32(lba),
			.transfer_length = scsi_uint32_to_data32(transfer_length),
		};
		command = scsi_create_command(device, &cmd, sizeof(cmd));
	} else {
		// use READ(10)
		scsi_read10_t cmd = {
			.opcode = SCSI_READ10_OPCODE,
			.flags  = flags,
			.lba = scsi_uint32_to_data32(lba),
			.transfer_length = scsi_uint16_to_data16(transfer_length),
		};
		command = scsi_create_command(device, &cmd, sizeof(cmd));
	}
	if (!command) return command;
	command->flags    = SCSI_CMD_READ_BUF;
	return command;
}

static const char *scsi_opcode2str(uint8_t command) {
#define COMMAND(opcode) case SCSI_ ## opcode ## _OPCODE: return #opcode;
	switch (command) {
	COMMAND(INQUIRY)
	COMMAND(READ_CAPACITY10)
	COMMAND(READ10)
	COMMAND(READ_TOC)
	COMMAND(READ12)
	COMMAND(READ_CAPACITY16)
	default:
		return "UNKNOWN";
	}
#undef COMMAND
}

void scsi_print_command(scsi_command_t *command) {
	kprintf("SCSI command ");
	kprintf("opcode=%02hhx(%s) ", command->data.opcode, scsi_opcode2str(command->data.opcode));
	kprintf("flags=%02hhx(", command->flags);
	int prev = 0;
#define FLAG(x) \
	if (command->flags & SCSI_CMD_ ## x) {\
		if (prev) kprintf(", "); \
		prev = 1; \
		kprintf(#x);\
	}
	FLAG(READ_BUF)
	FLAG(WRITE_BUF)
#undef FLAG
	kprintf(")");
}

static const char *scsi_peripheral2str(uint8_t peripheral) {
	switch (peripheral & SCSI_INQUIRY_PERIPHERAL_TYPE) {
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SBC4:
		return "SBC4 direct access block device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SSC3:
		return "SSC3 sequential access block device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SSC:
		return "SSC printer device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SPC2:
		return "SPC2 processor device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SBC_ALT:
		return "SBC write-once device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_MMC5:
		return "MMC5 CD/DVD device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SBC:
		return "SBC optical memory device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SMC3:
		return "SMC3 medium changer device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SCC2:
		return "SCC2 array storage controller";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_SES:
		return "SES enclosure services device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_RBC:
		return "RBC simplified direct access device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_OCRW:
		return "OCRW optical card reader/writer device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_BCC:
		return "BCC bridge controller device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_OSD:
		return "OSD object based storage device";
	case SCSI_INQUIRY_PERIPHERAL_TYPE_ADC2:
		return "ADC2 automation/drive interface";
	default:
		return "unknown";
	}
}

static void scsi_inquiry_str2str(char *dest, const char *src, size_t src_size) {
	for (size_t i = src_size - 1; i > 0; i--) {
		if (src[i] != ' ') break;
		src_size = i;
	}
	memcpy(dest, src, src_size);
	dest[src_size] = '\0';
}

scsi_device_t *scsi_create_device(devnode_t *bus) {
	scsi_device_t *device = kmalloc(sizeof(scsi_device_t));
	if (!device) return NULL;
	memset(device, 0, sizeof(scsi_device_t));
	device->bus = bus;

	scsi_inquiry_data_t ident;

	scsi_inquiry_t inquiry = {
		.opcode = SCSI_INQUIRY_OPCODE,
		.allocation_length = scsi_uint16_to_data16(sizeof(ident)),
	};

	scsi_command_t *command = scsi_create_command(device, &inquiry, sizeof(inquiry));
	if (!command) {
error:
		kfree(device);
		return NULL;
	}
	command->flags    = SCSI_CMD_READ_BUF;
	iobuf_init_continuous(&command->iobuf, &ident, sizeof(ident));

	int ret = ioreq_submit_sync(&command->ioreq);
	if (ret < 0) goto error;


	if ((ident.peripheral & SCSI_INQUIRY_PERIPHERAL_QUALIFIER) != SCSI_INQUIRY_PERIPHERAL_QUALIFIER_SUPPORTED) {
		// not connected
		goto error;
	}

	scsi_inquiry_str2str(device->info.product,  ident.product, sizeof(ident.product));
	scsi_inquiry_str2str(device->info.vendor,   ident.vendor, sizeof(ident.vendor));
	scsi_inquiry_str2str(device->info.firmware, ident.product_revision, sizeof(ident.product_revision));
	scsi_inquiry_str2str(device->info.serial,   ident.serial, sizeof(ident.serial));

	kinfof("found SCSI device %hhx(%s), vendor %s product %s\n", ident.peripheral, scsi_peripheral2str(ident.peripheral), device->info.vendor, device->info.product);

	device->type = ident.peripheral & SCSI_INQUIRY_PERIPHERAL_TYPE;
	bus_attach_child(bus, &device->devnode, NULL, UNIT_NOUNIT);
	return device;
}

int libscsi_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&scsi_commands_slab, sizeof(scsi_command_t), "scsi-commands");
	EXPORT(scsi_create_command);
	EXPORT(scsi_create_read_command);
	EXPORT(scsi_print_command);
	EXPORT(scsi_create_device);
	return 0;
}

int libscsi_fini(void) {
	slab_destroy(&scsi_commands_slab);
	UNEXPORT(scsi_create_command);
	UNEXPORT(scsi_create_read_command);
	UNEXPORT(scsi_print_command);
	UNEXPORT(scsi_create_device);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = libscsi_init,
	.fini        = libscsi_fini,
	.author      = "tayoky",
	.name        = "libscsi",
	.description = "common SCSI utils",
	.license     = "GPL 3",
};
