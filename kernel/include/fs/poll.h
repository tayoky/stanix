#ifndef _KERNEL_POLL_H
#define _KERNEL_POLL_H

#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/list.h>
#include <kernel/vfs.h>

typedef struct poll_event {
    list_node_t node;
    vfs_fd_t *fd;
    void *private;
    short events;
    short revents;
} poll_event_t;

typedef struct poll {
    spinlock_t lock;
    list_t events;
    int ready;
} poll_t;

void init_poll(void);

int poll_init(poll_t *poll);
void poll_fini(poll_t *poll);

/**
 * @brief wait for a poll to become ready
 * @param poll the poll to wait for
 * @param timeout an optional timeout
 */
int poll_wait(poll_t *poll, struct timeval *timeout);

/**
 * @brief cancel a poll
 * @param poll the poll to cancel
 * @note you must call poll_fini after
 */
void poll_cancel(poll_t *poll);


int poll_add(poll_t *poll, vfs_fd_t *fd, short events);

#endif
