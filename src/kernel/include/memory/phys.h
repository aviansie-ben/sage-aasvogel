#ifndef MEMORY_PHYS_H
#define MEMORY_PHYS_H

#include <typedef.h>
#include <lock.h>
#include <core/bootparam.h>

typedef enum
{
    MEM_BLOCK_FREE        = (1 << 0),
    MEM_BLOCK_HW_RESERVED = (1 << 1),
    MEM_BLOCK_LOCKED      = (1 << 2),
    MEM_BLOCK_KERNEL_ONLY = (1 << 3)
} mem_block_flags;

struct mem_region;
struct mem_block;

typedef struct mem_region
{
    spinlock lock;
    
    uint64 physical_address;
    
    uint16 num_blocks;
    uint16 first_free;
    
    struct mem_block* block_info;
    
    struct mem_region* next;
} mem_region;

typedef struct mem_block
{
    struct mem_region* region;
    uint16 next_free;
    
    uint16 ref_count;
    
    uint32 flags;
    uint32 owner_pid;
} mem_block;

void kmem_phys_init(const boot_param* param);

uint64 kmem_block_address(mem_block* block);
mem_block* kmem_block_find(uint64 address);
mem_block* kmem_block_next(mem_block* prev);

mem_block* kmem_block_alloc(bool kernel);
void kmem_block_free(mem_block* block);

void kmem_block_reserve(mem_block* start, mem_block* end, uint32 flags);

#endif
