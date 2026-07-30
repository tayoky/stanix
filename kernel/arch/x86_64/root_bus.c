#include <kernel/arch.h>

void init_root_bus(void) {
	kstatusf("init root bus...");
	bus_t *bus = kmalloc(sizeof(bus_t));
	memset(bus, 0, sizeof(bus_t));
	kok();
}
