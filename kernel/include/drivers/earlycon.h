#ifndef KERNEL_EARLYCON_H
#define KERNEL_EARLYCON_H

#include <kernel/list.h>
#include <sys/types.h>

typedef struct earlycon {
	list_node_t node;
	const char *name;
	ssize_t (*output)(struct earlycon *earlycon, const char *buf, size_t count);
} earlycon_t;

void earlycon_register(earlycon_t *earlycon);
void earlycon_unregister(earlycon_t *earlycon);
void earlycon_unregister_by_name(const char *name);
void earlycon_output(earlycon_t *earlycon, const char *buf, size_t count);
void earlycon_output_all(const char *buf, size_t count);
void earlycon_output_str(const char *str);

#endif
