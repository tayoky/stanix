#ifndef KERNEL_IOREQ_H
#define KERNEL_IOREQ_H

#include <kernel/oneshot.h>
#include <kernel/list.h>
#include <kernel/refcount.h>
#include <errno.h>

typedef struct ioreq ioreq_t;
typedef struct ioreq_ops ioreq_ops_t;
typedef struct ioreq_queue ioreq_queue_t;

struct ioreq {
	oneshot_t oneshot;
	list_node_t node;
	ref_count_t ref_count;
	ioreq_queue_t *queue; // only if the request is queued
	ioreq_ops_t *ops;
	void (*callback)(ioreq_t *ioreq, void *data);
	void *data;
	uintptr_t data2[3];
	int ret;
	int padding;
};

struct ioreq_ops {
	int (*submit)(ioreq_t *ioreq);
	void (*cancel)(ioreq_t *ioreq);
	void (*finish)(ioreq_t *ioreq);
	void (*cleanup)(ioreq_t *ioreq);
};

struct ioreq_queue {
	list_t pendings;
};

static inline ioreq_t *ioreq_ref(ioreq_t *ioreq) {
	ref_count_inc(&ioreq->ref_count);
	return ioreq;
}

static inline void ioreq_release(ioreq_t *ioreq) {
	if (ref_count_dec(&ioreq->ref_count) > 1) {
		return;
	}
	if (ioreq->ops && ioreq->ops->cleanup) {
		ioreq->ops->cleanup(ioreq);
	}
}

static inline void ioreq_set_callback(ioreq_t *ioreq, void (*callback)(ioreq_t *ioreq, void *data), void *data) {
	ioreq->callback = callback;
	ioreq->data     = data;
}

int ioreq_submit(ioreq_t *ioreq);

static inline void ioreq_cancel(ioreq_t *ioreq) {
	if (ioreq->ops && ioreq->ops->cancel) {
		ioreq->ops->cancel(ioreq);
	}
}

static inline int ioreq_wait(ioreq_t *ioreq) {
	oneshot_wait(&ioreq->oneshot);
	return ioreq->ret;
}

static inline int ioreq_wait_interruptible(ioreq_t *ioreq) {
	if (oneshot_wait_interruptible(&ioreq->oneshot) < 0) return -EINTR;
	return ioreq->ret;
}

int ioreq_submit_sync(ioreq_t *ioreq);

int ioreq_submit_sync_interruptible(ioreq_t *ioreq);

void ioreq_finish(ioreq_t *ioreq, int ret);

static inline void ioreq_queue(ioreq_t *ioreq, ioreq_queue_t *queue) {
	ioreq->queue = ioreq;
	list_append(&queue->pendings, &ioreq->node);
}

void ioreq_queue_submit_pending(ioreq_queue_t *queue);
#endif
