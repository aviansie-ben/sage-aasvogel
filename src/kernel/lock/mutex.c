#include <lock/mutex.h>
#include <lock/spinlock.h>

#include <core/crash.h>

void mutex_init(mutex* m)
{
    m->taken = 0;
    m->owner = NULL;
    m->owner_next = NULL;
    
    sched_thread_queue_init(&m->wait_queue);
}

static bool mutex_acquire_fast(mutex* m)
{
    uint32 expected = 0;
    return __atomic_compare_exchange_n(&m->taken, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

void mutex_acquire(mutex* m)
{
    sched_thread* t = sched_thread_current();
    uint32 eflags = eflags_save();
    asm volatile ("cli");
    
    if (m->owner == t)
        crash("Kernel mutex recursive locking detected!");
    
    if (!mutex_acquire_fast(m))
    {
        spinlock_acquire(&m->wait_queue.lock);
        
        if (!mutex_acquire_fast(m))
        {
            t->status = STS_BLOCKING;
            sched_thread_enqueue(&m->wait_queue, t);
            spinlock_release(&m->wait_queue.lock);
            sched_yield();
        }
        else
        {
            spinlock_release(&m->wait_queue.lock);
            
            m->owner = t;
            m->owner_next = t->held_mutexes;
            t->held_mutexes = m;
        }
    }
    else
    {
        m->owner = t;
        m->owner_next = t->held_mutexes;
        t->held_mutexes = m;
    }
    
    eflags_load(eflags);
}

bool mutex_try_acquire(mutex* m)
{
    if (m->owner == sched_thread_current())
        crash("Kernel mutex recursive locking detected!");
    
    return mutex_acquire_fast(m);
}

static void mutex_remove_from_held(sched_thread* t, mutex* m)
{
    mutex* pm = NULL;
    
    for (mutex* hm = t->held_mutexes; hm != NULL; pm = hm, hm = hm->owner_next)
    {
        if (hm == m)
        {
            if (pm == NULL)
                t->held_mutexes = hm->owner_next;
            else
                pm->owner_next = hm->owner_next;
            
            return;
        }
    }
    
    crash("Thread held_mutexes linked list corrupted");
}

void mutex_release(mutex* m)
{
    sched_thread* t = sched_thread_current();
    sched_thread* nt;
    
    if (m->owner != t)
        crash("Kernel mutex released by non-owner!");
    
    mutex_remove_from_held(t, m);
    
    spinlock_acquire(&m->wait_queue.lock);
    if ((nt = sched_thread_dequeue(&m->wait_queue)) != NULL)
    {
        m->owner = nt;
        m->owner_next = nt->held_mutexes;
        nt->held_mutexes = m;
        
        sched_thread_wake(nt);
    }
    else
    {
        m->owner = NULL;
        m->owner_next = NULL;
        m->taken = 0;
    }
    
    asm volatile ("mfence" : : : "memory");
    spinlock_release(&m->wait_queue.lock);
}
