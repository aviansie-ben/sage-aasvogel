/**
 * \file
 * \brief Physical memory allocation and management.
 * 
 * This file defines the necessary functions for allocating and managing physical memory frames.
 */

#ifndef MEMORY_PHYS_H
#define MEMORY_PHYS_H

#include <typedef.h>
#include <core/bootparam.h>

/**
 * \brief Value used to indicate the absence of a frame.
 * 
 * Take note that this value **does not** correspond to all bits being zero. An explicit comparison
 * with this value must be made to determine if a physical frame is null.
 */
#define FRAME_NULL 0xdeaddeaddeaddeadu

/**
 * \brief The number of bits at the lower end of the address are reserved for offsets within a
 *        single frame.
 */
#define FRAME_SHIFT 12

/**
 * \brief The size (in bytes) of a single physical frame for the current machine.
 */
#define FRAME_SIZE (1 << FRAME_SHIFT)

/**
 * \brief A mask which can be bitwise AND'ed with an address (physical or virtual) to get the frame
 *        offset of that address.
 */
#define FRAME_OFFSET_MASK (FRAME_SIZE - 1)

/**
 * \brief A mask which can be bitwise AND'ed with a physical address to get only the frame part of
 *        that address.
 */
#define FRAME_MASK ~(addr_p)(FRAME_OFFSET_MASK)

/**
 * \brief A value of the correct width to hold the largest possible physical address on the current
 *        machine.
 */
typedef uint64 addr_p;

/**
 * \brief Flags used to control physical memory allocation behaviour.
 * 
 * Combinations of these flags (made by using bitwise OR with the desired flags) can be passed to
 * \link kmem_frame_alloc \endlink to control various behaviour of frame allocation.
 */
typedef enum
{
    /**
     * \brief Allow the allocation request to block in order to wait for additional memory to be
     *        freed.
     * 
     * In the event that insufficient memory is currently available to service an allocation request
     * with this flag, the current thread will be blocked until sufficient physical memory is
     * available.
     * 
     * \warning This flag is only a suggestion to the memory allocator. Passing this flag does
     *          **not** guarantee that memory will always be returned. Always check the return value
     *          against \link FRAME_NULL \endlink before making use of the returned frame.
     */
    FA_WAIT = 0x1,
    
    /**
     * \brief Indicates that this memory request should use emergency memory reserves.
     * 
     * If insufficient memory is available in standard frame pools to service this request, frames
     * from the emergency frame pool will be used to complete the request.
     * 
     * Use of this flag should be limited to situations where failure to allocate memory may lead to
     * system instability or a system crash. Making large allocations using this flag is heavily
     * discouraged, since doing so may deplete the emergency frame pool, which could lead the memory
     * management subsystem to be unable to free memory, leading to a system crash.
     */
    FA_EMERG = 0x2,
    
    /**
     * \brief Indicates that this allocation request should only be fulfilled with frames whose
     *        physical addresses fit in 32 bits.
     * 
     * When passing this flag, all returned physical frames are guaranteed to have physical
     * addresses < 0x100000000.
     * 
     * Use of this allocation flag is discouraged except where necessary for interacting with
     * hardware, since the amount of available physical memory to service these requests may be
     * limited.
     */
    FA_32BIT = 0x4,
    
    /**
     * \brief Indicates that this allocation request should only be fulfilled with frames whose
     *        physical addresses are within the first 1MiB of memory.
     * 
     * When passing this flag, all returned physical frames are guaranteed to have physical
     * addresses < 0x100000.
     * 
     * Due to the way that frame pools are divided, the \link FA_EMERG \endlink flag cannot be used
     * in conjunction with this flag. Frames under 1MiB are always considered reserved for use with
     * either this flag or with other allocations using \link FA_EMERG \endlink.
     * 
     * \warning This allocation flag should not be used unless you are 100% sure that your use case
     *          requires its physical addresses to be <1MiB. There is very little space available in
     *          this region of memory, so any unnecessary use of this flag could lead to allocations
     *          which do require low addresses failing unnecessarily.
     */
    FA_LOW_MEM = 0x8
} frame_alloc_flags;

/**
 * \brief The total number of frames that are able to be used in this system.
 * 
 * In the event that certain memory is unusable (e.g. memory >4GiB with PAE disabled), this memory
 * will **not** be included in this total.
 */
extern uint32 kmem_total_frames;

/**
 * \brief The number of frames currently available for use to satisfy memory allocations.
 */
extern uint32 kmem_free_frames;

extern void kmem_phys_init(const boot_param* param) __hidden;

/**
 * \brief Allocates a physical frame.
 * 
 * This function will retrieve a frame from the current buffer of frames which are available for
 * allocation. On success, the physical address corresponding to the newly allocated frame is
 * returned. On failure, \link FRAME_NULL \endlink is returned.
 * 
 * Once the allocated frame is no longer required, it should be returned to the pool of available
 * frames for allocation by calling \link kmem_frame_free \endlink. Failure to do so may lead to
 * exhaustion of memory and other undesirable side effects.
 * 
 * \param flags Flags used to control the behaviour of the physical frame allocator. See the
 *              documentation on \link frame_alloc_flags \endlink for more information on the use of
 *              these flags.
 * 
 * \return If successful, the address of the physical frame was allocated. If unsuccessful,
 *         \link FRAME_NULL \endlink.
 */
extern addr_p kmem_frame_alloc(frame_alloc_flags flags) __warn_unused_result;

/**
 * \brief Returns a physical frame to the pool of available frames for allocation.
 * 
 * This function frees a frame previously allocated through the use of
 * \link kmem_frame_alloc \endlink or \link kmem_frame_alloc_many \endlink.
 * 
 * \warning It is the responsibility of the caller to ensure that the freed frame is no longer in
 *          use _before_ calling this function; this includes synchronization between multiple CPUs
 *          if necessary. Failure to do so may lead to possible security issues, since any leaked
 *          reference to freed memory may be used to modify the value of future structures allocated
 *          inside of the relevant frames.
 * 
 * \param frame The physical address of the frame which is being freed. This value **must** be a
 *              value previously returned by a frame allocation request and must not be
 *              \link FRAME_NULL \endlink.
 */
extern void kmem_frame_free(addr_p frame);

/**
 * \brief Allocates any number of physical frames.
 * 
 * This function will retrieve a number of frames from the current buffer of frames which are
 * available for allocation and will place them in the passed buffer.
 * 
 * In the event that insufficient memory is available to service the request, the number of frames
 * which could be allocated will be returned and that number of frames will be populated in the
 * passed buffer. It is the responsibility of the caller to free these frames if it cannot proceed
 * with a partial allocation.
 * 
 * This function **does not** provide any guarantee regarding the contiguity of the returned
 * physical frames.
 * 
 * When allocated frames are no longer required, they should be returned to the pool of available
 * frames for allocation by calling \link kmem_frame_free_many \endlink. Failure to do so may lead
 * to exhaustion of memory and other undersirable side effects.
 * 
 * \param[out] frames A buffer into which the addresses of the newly allocated frames should be
 *                    placed. The size of this buffer must be sufficient to hold at least
 *                    \c num_frames physical addresses. After this call returns, the return value
 *                    signifies the number of elements in this buffer (starting from the beginning
 *                    of the buffer) which are valid; all other entries in this buffer are left in
 *                    an undefined state after the call completes.
 * \param num_frames The number of frames which are being requested. After this function returns,
 *                   at most this many physical frames will have been allocated for the caller to
 *                   use.
 * \param flags Flags used to control the behaviour of the physical frame allocator. See the
 *              documentation on \link frame_alloc_flags \endlink for more information on the use of
 *              these flags.
 * 
 * \return The number of frames successfully allocated. This value will always be <= \c num_frames.
 */
extern size_t kmem_frame_alloc_many(addr_p* frames, size_t num_frames, frame_alloc_flags flags) __warn_unused_result;

/**
 * \brief Frees a number of previously allocated frames.
 * 
 * This function will free a number of previously allocated frames to allow them to be allocated
 * for other purposes. This function is subject to the same limitations as
 * \link kmem_frame_free \endlink.
 * 
 * \param[in] frames A buffer of \c num_frames physical addresses which should be freed. Frames in
 *                   this buffer may have been allocated using either
 *                   \link kmem_frame_alloc_many \endlink or \link kmem_frame_alloc \endlink.
 * \param num_frames The number of frames in the buffer which should be freed.
 * 
 * \sa kmem_frame_free
 */
extern void kmem_frame_free_many(const addr_p* frames, size_t num_frames);

#endif
