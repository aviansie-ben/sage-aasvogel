#ifndef LOCK_MUTEX_H
#define LOCK_MUTEX_H

#include <typedef.h>
#include <core/sched.h>

typedef struct mutex
{
    uint32 taken;
    
    sched_thread* owner;
    struct mutex* owner_next;
    
    sched_thread_queue wait_queue;
} mutex;

extern void mutex_init(mutex* m);

extern void mutex_acquire(mutex* m);
extern bool mutex_try_acquire(mutex* m);
extern void mutex_release(mutex* m);

#endif
