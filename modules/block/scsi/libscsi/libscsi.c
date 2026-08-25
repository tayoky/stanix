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
	return scsi_driver->submit_scsi_command(scsi_command->device->channel, scsi_command->device, scsi_command);
}

static void scsi_free_command(ioreq_t *ioreq) {
	scsi_command_t *scsi_command = container_of(ioreq, scsi_command_t, ioreq);
	slab_free(scsi_command);
}

static ioreq_ops_t scsi_command_ops = {
	.submit  = scsi_submit_command,
	.cleanup = scsi_free_command,
};

scsi_command_t *scsi_create_command(scsi_device_t *device, void *data, size_t size);
	scsi_driver_t *scsi_driver = container_of(device->channel->driver, scsi_driver_t, driver);
	if (!scsi_driver->submit_scsi_command) return NULL;
	scsi_command_t *command = slab_alloc(&scsi_commands_slab);
	if (!command) return NULL;
	memset(command, 0, sizeof(scsi_command_t));
	command->device = device;
	command->ioreq.ops = &scsi_command_ops;
	if (data) {
		kassert(size <= sizeof(command->data));
		memcpy(command->data, data, size);
	}
	return command;
}

static const char scsi_opcode2str(uint8_t command) {
#define COMMAND(opcode) case SCSI_CMD_ ## opcode: return #opcode;
	switch (command & SCSI_OPCODE_DATA) {
	COMMAND(INQUIRY)
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
	if (command->flag & SCSI_CMD_ ## x) {\
		if (prev) kprintf(", "); \
		prev = 1; \
		kprintf(#x);\
	}
	FLAG(READ_BUF)
	FLAG(WRITE_BUF)
#undef FLAG
	kprintf(")");
}

scsi_device_t *scsi_create_device(devnode_t *bus) {
	scsi_device_t *device = kmalloc(sizeof(scsi_device_t));
	if (!device) return NULL;
	device->bus = bus;

	scsi_inquiry_data_t ident;

	scsi_inquiry_t inquiry = {
		.opcode = SCSI_CMD_INQUIRY,
		.allocation_lenght = sizeof(ident),
	};

	scsi_command_t *command = scsi_create_command(device, &inquiry, sizeof(inquiry));
	command->buf_size = sizeof(ident);
	command->buf      = &ident;

	ioreq_submit_sync(command);

	kinfof("found SCSI device vendor %.8s product %.8s\n", ident.vendor, ident.product);

	// TODO : fill some stuff

	bus_attach_child(bus, &device->devnode, NULL, UNIT_NOUNIT);
	return device;
}

int libscsi_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&scsi_commands_slab, sizeof(scsi_command_t), "scsi-commands");
	EXPORT(scsi_create_command);
	EXPORT(scsi_print_command);
	EXPORT(scsi_create_device);
	return 0;
}

int libscsi_fini(void) {
	slab_destroy(&scsi_commands_slab);
	UNEXPORT(scsi_create_command);
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
