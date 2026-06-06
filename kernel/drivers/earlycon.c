#include <kernel/earlycon.h>
#include <kernel/list.h>
#include <kernel/string.h>

static list_t earlycons;

void earlycon_register(earlycon_t *earlycon) {
	list_append(&earlycons, &earlycon->node);
}

void earlycon_unregister(earlycon_t *earlycon) {
	list_remove(&earlycons, &earlycon->node);
}

void earlycon_unregister_by_name(const char *name) {
	foreach (node, &earlycons) {
		earlycon_t *earlycon = container_of(node, earlycon_t, node);
		if (!strcmp(earlycon->name, name)) {
			earlycon_unregister(earlycon);
		}
	}
}

void earlycon_output(earlycon_t *earlycon, const char *buf, size_t count) {
	earlycon->output(earlycon, buf, count);
}

void earlycon_output_all(const char *buf, size_t count) {
	foreach (node, &earlycons) {
		earlycon_t *earlycon = container_of(node, earlycon_t, node);
		earlycon_output(earlycon, buf, count);
	}
}

void earlycon_output_str(const char *str) {
	earlycon_output_all(str, strlen(str));
}
