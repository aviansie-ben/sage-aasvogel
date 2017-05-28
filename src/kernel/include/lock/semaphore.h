#ifndef LOCK_SEMAPHORE_H
#define LOCK_SEMAPHORE_H

#include <typedef.h>
#include <lock/spinlock.h>
#include <core/sched.h>

typedef struct semaphore
{
    spinlock lock;

    int value;
    sched_thread_queue wait_queue;
} semaphore;

extern void semaphore_init(semaphore* s, int value);

extern void semaphore_wait(semaphore* s);
extern bool semaphore_try_wait(semaphore* s);
extern void semaphore_signal(semaphore* s);

#endif
