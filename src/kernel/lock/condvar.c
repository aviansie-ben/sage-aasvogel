#include <lock/condvar.h>
#include <lock/spinlock.h>

#include <core/crash.h>

void cond_var_init(cond_var* v, mutex* m)
{
    v->lock = m;
    sched_thread_queue_init(&v->wait_queue);
}

void cond_var_wait(cond_var* v)
{
    sched_thread* t = sched_thread_current();

    uint32 eflags = eflags_save();
    asm volatile ("cli");

    if (v->lock != NULL && v->lock->owner != t)
        crash("Attempt to wait on a condition variable with an unowned lock!");

    spinlock_acquire(&v->wait_queue.lock);

    t->status = STS_BLOCKING;
    sched_thread_enqueue(&v->wait_queue, t);

    spinlock_release(&v->wait_queue.lock);
    if (v->lock != NULL) mutex_release(v->lock);

    sched_yield();
    if (v->lock != NULL) mutex_acquire(v->lock);

    eflags_load(eflags);
}

void cond_var_signal(cond_var* v)
{
    sched_thread* t;

    if (v->lock != NULL && v->lock->owner != sched_thread_current())
        crash("Attempt to signal a conditional variable with an unowned lock!");

    spinlock_acquire(&v->wait_queue.lock);

    if ((t = sched_thread_dequeue(&v->wait_queue)) != NULL)
    {
        sched_thread_wake(t);
    }

    spinlock_release(&v->wait_queue.lock);
}

void cond_var_broadcast(cond_var* v)
{
    sched_thread* t;

    if (v->lock != NULL && v->lock->owner != sched_thread_current())
        crash("Attempt to signal a conditional variable with an unowned lock!");

    spinlock_acquire(&v->wait_queue.lock);

    while ((t = sched_thread_dequeue(&v->wait_queue)) != NULL)
    {
        sched_thread_wake(t);
    }

    spinlock_release(&v->wait_queue.lock);
}

void cond_var_s_init(cond_var_s* v, spinlock* l)
{
    v->lock = l;
    sched_thread_queue_init(&v->wait_queue);
}

void cond_var_s_wait(cond_var_s* v, mutex* m)
{
    sched_thread* t = sched_thread_current();
    uint32 eflags;

    spinlock_acquire(&v->wait_queue.lock);

    t->status = STS_BLOCKING;
    sched_thread_enqueue(&v->wait_queue, t);

    spinlock_release(&v->wait_queue.lock);
    if (v->lock != NULL) eflags = _spinlock_release(v->lock);
    if (m != NULL) mutex_release(m);

    sched_yield();
    if (m != NULL) mutex_acquire(m);
    if (v->lock != NULL) _spinlock_acquire(v->lock, eflags);
}

void cond_var_s_signal(cond_var_s* v)
{
    sched_thread* t;

    spinlock_acquire(&v->wait_queue.lock);

    if ((t = sched_thread_dequeue(&v->wait_queue)) != NULL)
    {
        sched_thread_wake(t);
    }

    spinlock_release(&v->wait_queue.lock);
}

void cond_var_s_broadcast(cond_var_s* v)
{
    sched_thread* t;

    spinlock_acquire(&v->wait_queue.lock);

    while ((t = sched_thread_dequeue(&v->wait_queue)) != NULL)
    {
        sched_thread_wake(t);
    }

    spinlock_release(&v->wait_queue.lock);
}
