#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <typedef.h>
#include <lock/spinlock.h>

#include <memory/phys.h>

struct mempool_small_part;
typedef struct mempool_small_part mempool_small_part;

typedef struct mempool_small
{
    spinlock lock;
    const char* name;
    
    uint32 obj_size;
    
    uint32 frames_per_part;
    uint32 part_first_offset;
    
    uint32 num_total;
    uint32 num_free;
    
    mempool_small_part* parts_empty;
    mempool_small_part* parts_partial;
    mempool_small_part* parts_full;
    
    struct mempool_small* next;
} mempool_small;

extern void kmem_pool_small_init(mempool_small* pool, const char* name, uint32 obj_size, uint32 obj_align);
extern void* kmem_pool_small_alloc(mempool_small* pool, frame_alloc_flags flags) __warn_unused_result;
extern void kmem_pool_small_free(mempool_small* pool, void* obj);
extern void kmem_pool_small_compact(mempool_small* pool);

extern void kmem_pools_compact(void);

#endif
