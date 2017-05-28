#include <lock/rwlock.h>
#include <assert.h>

void rwlock_init(rwlock* l)
{
    spinlock_init(&l->lock);

    l->readers = 0;
    l->writers = 0;

    sched_thread_queue_init(&l->read_queue);
    sched_thread_queue_init(&l->write_queue);
}

void rwlock_acquire_read(rwlock* l)
{
    sched_thread* t = sched_thread_current();
    uint32 eflags = eflags_save();
    asm volatile ("cli");

    spinlock_acquire(&l->lock);

    if (l->writers == 0)
    {
        l->readers++;
        spinlock_release(&l->lock);
    }
    else
    {
        spinlock_acquire(&l->read_queue.lock);
        t->status = STS_BLOCKING;
        sched_thread_enqueue(&l->read_queue, t);
        spinlock_release(&l->read_queue.lock);
        spinlock_release(&l->lock);
        sched_yield();
    }

    eflags_load(eflags);
}

void rwlock_acquire_write(rwlock* l)
{
    sched_thread* t = sched_thread_current();
    uint32 eflags = eflags_save();
    asm volatile ("cli");

    spinlock_acquire(&l->lock);
    l->writers++;

    if (l->writers == 1 && l->readers == 0)
    {
        spinlock_release(&l->lock);
    }
    else
    {
        spinlock_acquire(&l->write_queue.lock);
        t->status = STS_BLOCKING;
        sched_thread_enqueue(&l->write_queue, t);
        spinlock_release(&l->write_queue.lock);
        spinlock_release(&l->lock);
        sched_yield();
    }

    eflags_load(eflags);
}

bool rwlock_try_acquire_read(rwlock* l)
{
    spinlock_acquire(&l->lock);

    if (l->writers == 0)
    {
        l->readers++;
        spinlock_release(&l->lock);

        return true;
    }
    else
    {
        spinlock_release(&l->lock);

        return false;
    }
}

bool rwlock_try_acquire_write(rwlock* l)
{
    spinlock_acquire(&l->lock);

    if (l->writers == 0)
    {
        l->writers++;
        spinlock_release(&l->lock);

        return true;
    }
    else
    {
        spinlock_release(&l->lock);

        return false;
    }
}

void rwlock_release_read(rwlock* l)
{
    sched_thread* t;

    spinlock_acquire(&l->lock);

    assert(l->readers != 0);
    l->readers--;

    if (l->readers == 0 && l->writers != 0)
    {
        spinlock_acquire(&l->write_queue.lock);

        t = sched_thread_dequeue(&l->write_queue);
        sched_thread_wake(t);

        spinlock_release(&l->write_queue.lock);
    }

    spinlock_release(&l->lock);
}

void rwlock_release_write(rwlock* l)
{
    sched_thread* t;

    spinlock_acquire(&l->lock);

    assert(l->readers == 0);
    assert(l->writers != 0);
    l->writers--;

    if (l->writers != 0)
    {
        spinlock_acquire(&l->write_queue.lock);

        t = sched_thread_dequeue(&l->write_queue);
        sched_thread_wake(t);

        spinlock_release(&l->write_queue.lock);
    }
    else
    {
        spinlock_acquire(&l->read_queue.lock);

        while (l->read_queue.first != NULL)
        {
            l->readers++;

            t = sched_thread_dequeue(&l->read_queue);
            sched_thread_wake(t);
        }

        spinlock_release(&l->read_queue.lock);
    }

    spinlock_release(&l->lock);
}
