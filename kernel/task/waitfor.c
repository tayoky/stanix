#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <sys/wait.h>
#include <stdatomic.h>
#include <errno.h>

//TODO : make this thread and SMP safe
//RACE CONDITION

//wait for any thread in a group to die
int waitfor(task **threads,size_t threads_count,int flags,task **waker){
    *waker = NULL;
    size_t waitfor_count = 0;
    for (size_t i=0; i<threads_count; i++) {
        task *expected = NULL;
        if(!atomic_compare_exchange_strong(&threads[i]->waiter, &expected, get_current_task())){
            //somebody is already waiting on it
            threads[i] = NULL;
            continue;
        }

        if(atomic_load(&threads[i]->flags) & PROC_FLAG_ZOMBIE){
            //already dead
            get_current_task()->waker = threads[i];
            goto ret;
        }

        //register
        waitfor_count++;
    }

    if(waitfor_count == 0){
        return -ECHILD;
    }

    int status = 0;

    if (flags & WNOHANG) {
        status = -ECHILD;
    } else if (block_task() < 0) {
        status = -EINTR;
    }

    ret:

    for (size_t i=0; i<threads_count; i++) {
       if(!threads[i])continue;

        //unregister
        task *expected = get_current_task();
        atomic_compare_exchange_strong(&threads[i]->waiter, &expected, NULL);
    }

    if (status != 0) {
        return status;
    }

    *waker = get_current_task()->waker;
    return get_current_task()->waker->tid;
}
