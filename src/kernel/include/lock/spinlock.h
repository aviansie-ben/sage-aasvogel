/**
 * \file
 * TODO: Add file description
 */

#ifndef LOCK_SPINLOCK_H
#define LOCK_SPINLOCK_H

#include <typedef.h>

/**
 * \brief A simple spinlock which can be acquired and released atomically.
 * 
 * If a spinlock is placed into memory which is allocated at runtime, it must be initialized before
 * use by using the \link spinlock_init \endlink function **once** when it is first created.
 * 
 * Contrary to other locking mechanisms, spinlocks are owned by **processors** and not by
 * **threads** or **processes**. Care should be taken to avoid any blocking operations while holding
 * a spinlock, as the spinlock will not be released for other processors if the scheduler is
 * invoked.
 * 
 * Except where doing so is impossible or impractical, other locking mechanisms like mutexes should
 * be used instead of spinlocks, as such mechanisms will release the CPU if they cannot acquire the
 * lock rather than busy-waiting.
 */
typedef struct
{
    /**
     * \brief A value that represents whether or not the spinlock is currently taken.
     * 
     * A value of 0 in this field represents a spinlock which is not currently held, while all other
     * values represent a spinlock which is currently held by a processor.
     * 
     * The value of this field should never be read or written to directly, as it requires special
     * atomic operations to work properly. Instead, make use of \link spinlock_acquire \endlink,
     * \link spinlock_release \endlink, and \link spinlock_try_acquire \endlink to indirectly modify
     * this value.
     */
    uint32 taken;
} spinlock;

/**
 * \brief Returns the value currently in the EFLAGS register.
 * 
 * This function is generally used to save the interrupt flag on the EFLAGS register before
 * disabling interrupts for some reason. The value returned should be considered meaningless, other
 * than the fact that it can be passed into \link eflags_load \endlink to restore interrupts to
 * their previous value.
 */
uint32 eflags_save(void);

/**
 * \brief Restores the provided value into the EFLAGS register.
 * 
 * This function should **only be used** to restore the EFLAGS register as stored by
 * \link eflags_save \endlink, as it will overwrite **all bits** in the EFLAGS register.
 */
void eflags_load(uint32 eflags);

/**
 * \brief Initializes the given spinlock to a state in which it is not held.
 * 
 * \param spinlock The spinlock which should be initialized.
 */
void spinlock_init(spinlock* lock);

/**
 * \brief Acquires the given spinlock. If the spinlock is already held, waits until it is release
 *        to acquire it.
 * 
 * While the spinlock is held, all interrupts will be disabled on the current processor to ensure
 * that an interrupt handler will not attempt to acquire the same spinlock.
 * 
 * \warning Calling this function to acquire a spinlock that you already hold will cause the
 *          processor to hang, as it is waiting for itself to release the spinlock. Interrupts
 *          **must** remain disabled while holding a spinlock, otherwise an interrupt handler may
 *          attempt to acquire a spinlock which the current processor already holds, which will
 *          result in this behaviour.
 * 
 * \warning While holding a spinlock, do not perform any operations that could potentially block.
 *          Spinlocks are not released when a thread is suspended, which could result in the new
 *          thread deadlocking if it attempts to acquire the same spinlock.
 * 
 * \param spinlock The spinlock to acquire.
 */
void spinlock_acquire(spinlock* lock);

/**
 * \brief Attempts to acquire the given spinlock. If the spinlock is already held, gives up and
 *        returns false.
 * 
 * This function will never busy-wait to acquire a spinlock, and will simply return immediately if
 * it fails to acquire the spinlock on the first attempt.
 * 
 * \param spinlock The spinlock to attempt to acquire.
 * 
 * \return true if the spinlock was acquired successfully, false otherwise.
 */
bool spinlock_try_acquire(spinlock* lock) __warn_unused_result;

/**
 * \brief Releases the given spinlock, allowing another processor to acquire it.
 * 
 * This function also restores the interrupt flag from when this spinlock was acquired, which will
 * cause interrupts to be enabled if they were enabled when acquiring the spinlock.
 * 
 * \warning A call to this function must always be preceeded by a corresponding call to either
 *          \link spinlock_acquire \endlink or \link spinlock_try_acquire \endlink. Attempting to
 *          release a spinlock you do not own will result in undefined behaviour.
 * 
 * \warning Spinlocks **must** be released in the reverse order in which they were acquired. This is
 *          a result of the fact that the state of the EFLAGS register is stored when acquiring a
 *          spinlock and is restored when releasing the same spinlock. Releasing spinlocks in the
 *          incorrect order could result in the EFLAGS register being in an unexpected state, which
 *          could result in interrupts being disabled when they shouldn't.
 * 
 * \param spinlock The spinlock to release.
 */
void spinlock_release(spinlock* lock);

#endif
