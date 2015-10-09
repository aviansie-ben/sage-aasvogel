#ifndef LOCK_SPINLOCK_H
#define LOCK_SPINLOCK_H

#include <typedef.h>

typedef struct
{
    uint32 taken;
} spinlock;

uint32 eflags_save(void);
void eflags_load(uint32 eflags);

void spinlock_init(spinlock* lock);

void spinlock_acquire(spinlock* lock);
bool spinlock_try_acquire(spinlock* lock);
void spinlock_release(spinlock* lock);

#endif
