#include <kernel/poll.h>
#include <kernel/vfs.h>
#include <kernel/string.h>
#include <kernel/slab.h>

static slab_cache_t poll_events_slab;

int poll_init(poll_t *poll) {
    memset(poll, 0, sizeof(poll_t));
    spinlock_acquire(&poll->lock);
}

void poll_fini(poll_t *poll) {

}

int poll_wait(poll_t *poll, struct timespec timeout) {
    block_prepare_interruptible();
    spinlock_release(&poll->lock);
    poll_fini(poll);
}

int poll_add(poll_t *poll, vfs_fd_t *fd, short events) {
    poll_event_t *event = slab_alloc(&poll_events_slab);
    event->revents = 0;
    event->events = events;
    event->fd = fd;
    int ret = vfs_poll_add(fd, event);
    if (ret < 0) return ret;

    list_append(&poll->events, &event->node);
    return 0;
}
