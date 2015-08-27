#ifndef LOCK_RWLOCK_H
#define LOCK_RWLOCK_H

#include <typedef.h>
#include <lock/spinlock.h>
#include <core/sched.h>

typedef struct rwlock
{
    spinlock lock;
    
    uint32 readers;
    uint32 writers;
    
    sched_thread_queue read_queue;
    sched_thread_queue write_queue;
} rwlock;

extern void rwlock_init(rwlock* lock);

extern void rwlock_acquire_read(rwlock* lock);
extern void rwlock_acquire_write(rwlock* lock);

extern bool rwlock_try_acquire_read(rwlock* lock);
extern bool rwlock_try_acquire_write(rwlock* lock);

extern void rwlock_release_read(rwlock* lock);
extern void rwlock_release_write(rwlock* lock);

#endif
