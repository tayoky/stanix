#include <kernel/arch.h>
#include <kernel/irq.h>

static void *root_register_handler(bus_t *bus, devnode_t *devnode, resource_t *resource, interrupt_handler_t handler, void *data) {
}

static bus_ops_t *root_ops = {
	.register_handler = root_register_handler,
};

void init_root_bus(void) {
	kstatusf("init root bus...");
	bus_t *bus = kmalloc(sizeof(bus_t));
	memset(bus, 0, sizeof(bus_t));
	bus->device.name = "root";
	bus->device.type = DEVICE_BUS,
	bus->ops = &root_ops;
	set_root_bus(bus);
	kok();
}
