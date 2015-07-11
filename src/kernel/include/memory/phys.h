#ifndef MEMORY_PHYS_H
#define MEMORY_PHYS_H

#include <typedef.h>
#include <core/bootparam.h>

#define FRAME_NULL 0xdeaddeaddeaddeadu
#define FRAME_SIZE 0x1000

typedef uint64 addr_p;

typedef enum
{
    FA_WAIT = 0x1,
    FA_EMERG = 0x2,
} frame_alloc_flags;

extern uint32 kmem_total_frames;
extern uint32 kmem_free_frames;

extern void kmem_phys_init(const boot_param* param);

extern addr_p kmem_frame_alloc(frame_alloc_flags flags);
extern void kmem_frame_free(addr_p frame);

extern size_t kmem_frame_alloc_many(addr_p* frames, size_t num_frames, frame_alloc_flags flags);
extern void kmem_frame_free_many(const addr_p* frames, size_t num_frames);

#endif
