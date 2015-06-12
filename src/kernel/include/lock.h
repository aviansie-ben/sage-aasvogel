#ifndef LOCK_H
#define LOCK_H

#include <typedef.h>

typedef struct
{
    uint32 taken;
} spinlock;

void spinlock_init(spinlock* lock);

void spinlock_acquire(spinlock* lock);
bool spinlock_try_acquire(spinlock* lock);
void spinlock_release(spinlock* lock);

#endif
