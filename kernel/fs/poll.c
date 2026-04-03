#include <kernel/poll.h>
#include <kernel/vfs.h>
#include <kernel/string.h>
#include <kernel/slab.h>

static slab_cache_t poll_events_slab;

void init_poll(void) {
    slab_init(&poll_events_slab, sizeof(poll_event_t), "poll events");
}

int poll_init(poll_t *poll) {
    memset(poll, 0, sizeof(poll_t));
    spinlock_acquire(&poll->lock);
    block_prepare_interruptible();
    return 0;
}

void poll_fini(poll_t *poll) {
    for (list_node_t *current=poll->events.first_node; current;) {
        poll_event_t *event = container_of(current, poll_event_t, node);
        current = current->next;

        list_remove(&poll->events, &event->node);
        slab_free(event);
    }
}

void poll_cancel(poll_t *poll) {
    block_cancel();

    // remove from each sleep queue
    foreach (current, &poll->events) {
        poll_event_t *event = container_of(current, poll_event_t, node);
        vfs_poll_remove(event->fd, event);
        vfs_poll_get(event->fd, event);
    }
}

int poll_wait(poll_t *poll, struct timespec *timeout) {
    int ret = 0;
    if (poll->ready) {
        block_cancel();
    } else {
        spinlock_release(&poll->lock);
        ret = block_task_timeout(timeout);
        spinlock_acquire(&poll->lock);
    }

    // remove from each sleep queue
    foreach (current, &poll->events) {
        poll_event_t *event = container_of(current, poll_event_t, node);
        vfs_poll_remove(event->fd, event);
        vfs_poll_get(event->fd, event);
    }
    return ret;
}

int poll_add(poll_t *poll, vfs_fd_t *fd, short events) {
    poll_event_t *event = slab_alloc(&poll_events_slab);
    event->revents = 0;
    event->events = events;
    event->fd = fd;
    list_append(&poll->events, &event->node);
    int ret = vfs_poll_get(fd, event);
    if (ret < 0) {
        event->revents |= POLLERR;
        poll->ready = 1;
        return ret;
    }
    if (event->revents) {
        // fast path
        // already ready
        poll->ready = 1;
        return 0;
    }
    ret = vfs_poll_add(fd, event);
    if (ret < 0) {
        event->revents |= POLLERR;
        poll->ready = 1;
        return ret;
    }
    ret = vfs_poll_get(fd, event);
    if (ret < 0) {
        event->revents |= POLLERR;
        poll->ready = 1;
        return ret;
    }
    if (event->revents) {
        // already ready
        poll->ready = 1;
        return 0;
    }

    return 0;
}
