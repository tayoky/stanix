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
    for(size_t i=0; i<threads_count; i++){
        if(atomic_load(&threads[i]->flags) & PROC_FLAG_ZOMBIE){
            //already dead
            get_current_task()->waker = threads[i];
            goto ret;
        }
        if(threads[i]->waiter){
            //somebody is already waiting on it
            threads[i] = NULL;
            continue;
        }

        //register
        if(!(flags & WNOHANG))threads[i]->waiter = get_current_task();
    }

    if(waitfor_count == 0 || (flags & WNOHANG)){
        return -ECHILD;
    }

    if(block_task() < 0){
        return -EINTR;
    }

    ret:

    for(size_t i=0; i<threads_count; i++){
       if(!threads[i])continue;

        //unregister
        if(threads[i]->waiter == get_current_task())threads[i]->waiter = NULL;
    }

    *waker = get_current_task()->waker;
    return get_current_task()->waker->tid;
}