#ifndef LOCK_CONDVAR_H
#define LOCK_CONDVAR_H

#include <typedef.h>
#include <core/sched.h>
#include <lock/mutex.h>

typedef struct cond_var
{
    mutex* lock;
    sched_thread_queue wait_queue;
} cond_var;

void cond_var_init(cond_var* v, mutex* m);
void cond_var_wait(cond_var* v);
void cond_var_signal(cond_var* v);
void cond_var_broadcast(cond_var* v);

typedef struct cond_var_s
{
    spinlock* lock;
    sched_thread_queue wait_queue;
} cond_var_s;

void cond_var_s_init(cond_var_s* v, spinlock* l);
void cond_var_s_wait(cond_var_s* v, mutex* m);
void cond_var_s_signal(cond_var_s* v);
void cond_var_s_broadcast(cond_var_s* v);

#endif
