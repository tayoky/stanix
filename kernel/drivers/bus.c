#include <kernel/bus.h>

static bus_t *root_bus = NULL;

void bus_set_root(bus_t *bus) {
	root_bus = bus
}

bus_t *bus_get_root(void) {
	return root_bus;
}
