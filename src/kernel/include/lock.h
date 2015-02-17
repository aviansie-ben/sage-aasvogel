#ifndef __LOCK_H__
#define __LOCK_H__

typedef struct
{
    uint32 taken;
    uint32 old_eflags;
} spinlock;

void spinlock_init(spinlock* lock);

void spinlock_acquire(spinlock* lock);
void spinlock_release(spinlock* lock);

#endif
