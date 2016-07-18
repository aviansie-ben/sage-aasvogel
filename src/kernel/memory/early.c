#include <memory/early.h>
#include <assert.h>

#include <memory/phys.h>
#include <memory/page.h>

#include <core/elf.h>

static bool init_done = false;
static bool final_done = false;

addr_v kmem_early_next_alloc;
addr_v kmem_early_min_alloc;

extern const void _ld_kmalloc_early_begin;
extern const void _ld_kmalloc_early_end;

static bool verify_mask(size_t mask)
{
    while ((mask & 0x1) != 0x0) mask >>= 1;
    
    return mask == 0x0;
}

void* kmalloc_early(size_t size, size_t align, uint32* physical_address)
{
    uint32 r;
    size_t align_mask = (align < 0x4) ? 0x3 : (align - 1);
    
    assert(init_done && !final_done);
    assert(verify_mask(align_mask));
    assert(kmem_early_next_alloc >= kmem_early_min_alloc);
    
    if ((addr_v)&_ld_kmalloc_early_end - kmem_early_next_alloc < size)
        crash("kmalloc_early out of memory");
    
    r = kmem_early_next_alloc;
    if ((r & align_mask) != 0x0)
    {
        r |= align_mask;
        r += 1;
    }
    
    assert(r >= kmem_early_next_alloc && (r & align_mask) == 0x0);
    kmem_early_next_alloc = r + size;
    
    if (physical_address != NULL)
        *physical_address = r - 0xC0000000;
    
    return (void*)r;
}

static inline void ensure_min_alloc_gte(uint32 addr)
{
    if (kmem_early_min_alloc < addr)
        kmem_early_min_alloc = addr;
}

void kmem_early_init(void)
{
    assert(!init_done && !final_done);
    
    kmem_early_min_alloc = (uint32)&_ld_kmalloc_early_begin;
    kmem_early_next_alloc = kmem_early_min_alloc;
    init_done = true;
}

void kmem_early_finalize(void)
{
    assert(init_done && !final_done);
    
    // Once we've finalized, the early memory manager can no longer allocate any
    // new memory.
    final_done = true;
    
    // The next allocation address should be page-aligned now.
    if ((kmem_early_next_alloc & FRAME_MASK) != 0)
    {
        kmem_early_next_alloc |= FRAME_MASK;
        kmem_early_next_alloc += 1;
    }
}
