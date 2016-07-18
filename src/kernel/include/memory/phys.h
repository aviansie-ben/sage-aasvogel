#ifndef MEMORY_PHYS_H
#define MEMORY_PHYS_H

#include <typedef.h>
#include <core/bootparam.h>

#define FRAME_NULL 0xdeaddeaddeaddeadu

#define FRAME_SHIFT 12
#define FRAME_SIZE (1 << FRAME_SHIFT)
#define FRAME_MASK (FRAME_SIZE - 1)

typedef uint64 addr_p;

typedef enum
{
    FA_WAIT = 0x1,
    FA_EMERG = 0x2,
    FA_32BIT = 0x4,
    FA_LOW_MEM = 0x8
} frame_alloc_flags;

extern uint32 kmem_total_frames;
extern uint32 kmem_free_frames;

extern void kmem_phys_init(const boot_param* param) __hidden;

extern addr_p kmem_frame_alloc(frame_alloc_flags flags) __warn_unused_result;
extern void kmem_frame_free(addr_p frame);

extern size_t kmem_frame_alloc_many(addr_p* frames, size_t num_frames, frame_alloc_flags flags) __warn_unused_result;
extern void kmem_frame_free_many(const addr_p* frames, size_t num_frames);

#endif
