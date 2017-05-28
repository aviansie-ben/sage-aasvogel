/**
 * \file
 * \brief Kernel mode mutexes for thread-based mutual exclusion.
 *
 * This file defines structs and functions necessary for the use of kernel-mode mutexes. These
 * mutexes allow for thread-based synchronization through the principle of mutual exclusion.
 */

#ifndef LOCK_MUTEX_H
#define LOCK_MUTEX_H

#include <typedef.h>
#include <core/sched.h>

/**
 * \brief A simple mutex which can be acquired and released atomically.
 *
 * Before being used, a mutex must be initialized using \link mutex_init \endlink.
 *
 * Note that mutexes are held by **threads** and not by **processors**. For this reason, mutexes
 * should never be acquired when in the context of an interrupt handler.
 *
 * These mutexes are not re-entrant. Attempting to acquire a mutex that is already held by the
 * current thread will result in the kernel crashing.
 *
 * No fields on this structure should ever be accessed directly, as they need to be handled using
 * special atomic operations. Instead, functions such as \link mutex_acquire \endlink should be used
 * to indirectly modify the state of the mutex.
 */
typedef struct mutex
{
    uint32 taken;

    sched_thread* owner;
    struct mutex* owner_next;

    sched_thread_queue wait_queue;
} mutex;

/**
 * \brief Initializes the given mutex to the unheld state.
 *
 * \param m The mutex which should be initialized.
 */
extern void mutex_init(mutex* m);

/**
 * \brief Attempts to acquire the given mutex and blocks if the mutex is currently held.
 *
 * This method may block the current thread, and can only be called in a context where doing so is
 * appropriate.
 *
 * Unlike some other mutex implementations, acquiring a mutex using this function is done using a
 * FIFO queue in the event of contention. This means that it is guaranteed that kernel mutexes are
 * inherently fair and do not suffer from starvation issues as a result of undefined acquiring
 * order.
 *
 * \warning Kernel mutexes are not re-entrant. Attempting to acquire a mutex which is already held
 *          by the calling thread will result in a system crash. Unlike spinlocks, however, doing so
 *          will not deadlock the system.
 *
 * \param m The mutex which is being acquired.
 */
extern void mutex_acquire(mutex* m);

/**
 * \brief Attempts to acquire the given mutex.
 *
 * Unlike \link mutex_acquire \endlink, this method will not block if the mutex is already held.
 * While it is okay to call this function from a context in which the thread cannot be blocked, e.g.
 * an interrupt handler, repeatedly attempting to acquire a mutex until it succeeds **does not**
 * guarantee that the mutex will be released and should be avoided.
 *
 * \warning Kernel mutexes are not re-entrant. Attempting to acquire a mutex which is already held
 *          by the calling thread will result in a system crash. Unlike spinlocks, however, doing so
 *          will not deadlock the system.
 *
 * \param m The mutex which is being acquired.
 *
 * \return true if the mutex was successfully acquired, and false if the mutex was not successfully
 *         acquired because it is already held.
 */
extern bool mutex_try_acquire(mutex* m);

/**
 * \brief Releases the given mutex, allowing another thread to acquire it.
 *
 * If there are any threads in the waiting queue that are awaiting the chance to acquire this mutex,
 * then the mutex will be given straight to them and will not be sent through any intermediary state
 * in which it is available to any other threads.
 *
 * \param m The mutex which is being released.
 */
extern void mutex_release(mutex* m);

#endif
