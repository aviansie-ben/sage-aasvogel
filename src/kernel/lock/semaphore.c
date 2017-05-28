#include <lock/semaphore.h>

void semaphore_init(semaphore* s, int value)
{
    spinlock_init(&s->lock);

    s->value = value;
    sched_thread_queue_init(&s->wait_queue);
}

void semaphore_wait(semaphore* s)
{
    sched_thread* t = sched_thread_current();
    uint32 eflags = eflags_save();
    asm volatile ("cli");

    spinlock_acquire(&s->lock);

    if (s->value-- <= 0)
    {
        spinlock_acquire(&s->wait_queue.lock);
        t->status = STS_BLOCKING;
        sched_thread_enqueue(&s->wait_queue, t);
        spinlock_release(&s->wait_queue.lock);
        spinlock_release(&s->lock);
        sched_yield();
    }
    else
    {
        spinlock_release(&s->lock);
    }

    eflags_load(eflags);
}

bool semaphore_try_wait(semaphore* s)
{
    spinlock_acquire(&s->lock);

    if (s->value > 0)
    {
        s->value--;
        spinlock_release(&s->lock);
        return true;
    }
    else
    {
        spinlock_release(&s->lock);
        return false;
    }
}

void semaphore_signal(semaphore* s)
{
    sched_thread* t;

    spinlock_acquire(&s->lock);

    if (s->value++ < 0)
    {
        spinlock_acquire(&s->wait_queue.lock);

        t = sched_thread_dequeue(&s->wait_queue);
        sched_thread_wake(t);

        spinlock_release(&s->wait_queue.lock);
    }

    spinlock_release(&s->lock);
}
