#include <kernel/module.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/bus.h>
#include <module/ata.h>

static slab_cache_t ata_commands_slab;

static void ata2str(char *dest, const char *atastr, size_t size) {
	for (size_t i=0; i<size; i+=2) {
		dest[i + 1] = atastr[i];
		dest[i]     = atastr[i + 1];
	}
	for (size_t i=size-1; i>0; i--) {
		if (dest[i] != ' ') {
			break;
		}
		dest[i] = '\0';
	}
	// always null terminate just in case
	dest[size] = '\0';
}

void ata_parse_common_ident(ata_common_ident_t *common_ident, ata_ident_t *ident) {
	ata2str(common_ident->model,    ident->model,    sizeof(ident->model));
	ata2str(common_ident->firmware, ident->firmware, sizeof(ident->firmware));
	ata2str(common_ident->serial,   ident->serial,   sizeof(ident->serial));
	common_ident->command_sets = ident->command_sets;
}

static int ata_submit_or_queue_command(ata_command_t *command) {
	int ret = ioreq_submit(&command->ioreq);
	if (ret == -EAGAIN) {
		// request cannot be send for the moment,
		// queue it
		list_append(&command->device->pending_commands, &command->node);
		ret = 0;
	}
	return ret;
}

static int ata_submit_command(ioreq_t *ioreq) {
	ata_command_t *ata_command = container_of(ioreq, ata_command_t, ioreq);
	ata_driver_t *ata_driver = container_of(ata_command->device->channel->driver, ata_driver_t, driver);
	return ata_driver->submit_ata_command(ata_command->device->channel, ata_command->device, ata_command);
}

static void ata_finish_command(ioreq_t *ioreq) {
	ata_command_t *ata_command = container_of(ioreq, ata_command_t, ioreq);
	ata_device_t *device = ata_command->device;

	// resubmit pending requests
	// NOTE : we do not need to guarantee that if the request 
	// is once again made pending, it return to the top of the queue
	// since this queue is only used for configuration commands
	if (list_is_empty(&device->pending_commands)) {
		return;
	}
	ata_command_t *pending_command = container_of(device->pending_commands.first_node, ata_command_t, node);
	list_remove(&device->pending_commands, &pending_command->node);
	int ret = ata_submit_or_queue_command(pending_command);
	if (ret < 0) {
		ioreq_finish(&pending_command->ioreq, ret);
	}
}

static void ata_free_command(ioreq_t *ioreq) {
	ata_command_t *ata_command = container_of(ioreq, ata_command_t, ioreq);
	slab_free(ata_command);
}

static ioreq_ops_t ata_command_ops = {
	.submit  = ata_submit_command,
	.finish  = ata_finish_command,
	.cleanup = ata_free_command,
};

ata_command_t *ata_create_command(ata_device_t *device) {
	ata_driver_t *ata_driver = container_of(device->channel->driver, ata_driver_t, driver);
	if (!ata_driver->submit_ata_command) return NULL;
	ata_command_t *command = slab_alloc(&ata_commands_slab);
	if (!command) return NULL;
	memset(command, 0, sizeof(ata_command_t));
	command->device = device;
	command->ioreq.ops = &ata_command_ops;
	return command;
}

int ata_submit_command_sync(ata_command_t *command) {
	ioreq_ref(&command->ioreq);
	int ret = ata_submit_or_queue_command(command);
	if (ret >= 0) {
		ret = ioreq_wait(&command->ioreq);
	}
	ioreq_release(&command->ioreq);
	return ret;
}

int libata_init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	slab_init(&ata_commands_slab, sizeof(ata_command_t), "ata-commands");
	EXPORT(ata_create_command);
	EXPORT(ata_submit_command_sync);
	EXPORT(ata_parse_common_ident);
	return 0;
}

int libata_fini(void) {
	slab_destroy(&ata_commands_slab);
	UNEXPORT(ata_create_command);
	UNEXPORT(ata_submit_command_sync);
	UNEXPORT(ata_parse_common_ident);
	return 0;
}

kmodule_t module_meta = {
	.magic       = MODULE_MAGIC,
	.init        = libata_init,
	.fini        = libata_fini,
	.author      = "tayoky",
	.name        = "libata",
	.description = "common ATA utils",
	.license     = "GPL 3",
};
