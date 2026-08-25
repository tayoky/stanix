#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/bus.h>
#include <module/scsi.h>

static slab_cache_t scsi_commands_slab;

static int scsi_submit_or_queue_command(scsi_command_t *command) {
	int ret = ioreq_submit(&command->ioreq);
	if (ret == -EAGAIN) {
		// request cannot be send for the moment,
		// queue it
		list_append(&command->device->pending_commands, &command->node);
		ret = 0;
	}
	return ret;
}

static int scsi_submit_command(ioreq_t *ioreq) {
	scsi_command_t *scsi_command = container_of(ioreq, scsi_command_t, ioreq);
	scsi_driver_t *scsi_driver = container_of(scsi_command->device->bus->driver, scsi_driver_t, driver);
	kdebugf("send ");
	scsi_print_command(scsi_command);
	kprintf("\n");
	return scsi_driver->submit_scsi_command(scsi_command->device->channel, scsi_command->device, scsi_command);
}

static void scsi_finish_command(ioreq_t *ioreq) {
	scsi_command_t *scsi_command = container_of(ioreq, scsi_command_t, ioreq);
	scsi_device_t *device = scsi_command->device;

	// resubmit pending requests
	// NOTE : we do not need to guarantee that if the request 
	// is once again made pending, it return to the top of the queue
	// since this queue is only used for configuration commands
	if (list_is_empty(&device->pending_commands)) {
		return;
	}
	scsi_command_t *pending_command = container_of(device->pending_commands.first_node, scsi_command_t, node);
	list_remove(&device->pending_commands, &pending_command->node);
	int ret = scsi_submit_or_queue_command(pending_command);
	if (ret < 0) {
		ioreq_finish(&pending_command->ioreq, ret);
	}
}

static void scsi_free_command(ioreq_t *ioreq) {
	scsi_command_t *scsi_command = container_of(ioreq, scsi_command_t, ioreq);
	slab_free(scsi_command);
}

static ioreq_ops_t scsi_command_ops = {
	.submit  = scsi_submit_command,
	.finish  = scsi_finish_command,
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

int scsi_submit_command_sync(scsi_command_t *command) {
	ioreq_ref(&command->ioreq);
	int ret = scsi_submit_or_queue_command(command);
	if (ret >= 0) {
		ret = ioreq_wait(&command->ioreq);
	}
	ioreq_release(&command->ioreq);
	return ret;
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

	// TODO : send inquiry and fill some stuff

	bus_attach_child(bus, &device->devnode, NULL, UNIT_NOUNIT);
	return device;
}

int libscsi_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&scsi_commands_slab, sizeof(scsi_command_t), "scsi-commands");
	EXPORT(scsi_create_command);
	EXPORT(scsi_submit_command_sync);
	EXPORT(scsi_print_command);
	EXPORT(scsi_create_device);
	return 0;
}

int libscsi_fini(void) {
	slab_destroy(&scsi_commands_slab);
	UNEXPORT(scsi_create_command);
	UNEXPORT(scsi_submit_command_sync);
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
