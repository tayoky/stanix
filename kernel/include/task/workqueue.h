#ifndef KERNEL_WORKQUEUE_H
#define KERNEL_WORKQUEUE_H

#include <kernel/list.h>
#include <kernel/string.h>

typedef struct work work_t;

struct work {
	list_node_t node; // protectedd by the workqueue lock
	void (*callback)(work_t *work);
	int is_queued;    // protectedd by the workqueue lock
};

void init_workqueue(void);

static inline void work_init(work_t *work, void (*callback)(work_t *work)) {
	memset(work, 0, sizeof(work_t));
	work->callback = callback;
}
void work_queue(work_t *work);

#endif
