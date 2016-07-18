#include <memory/early.h>
#include <assert.h>

#include <memory/phys.h>
#include <memory/page.h>

#include <core/elf.h>
#include <core/klog.h>
#include <core/unwind.h>
#include <core/ksym.h>

static bool init_done = false;
static bool final_done = false;

addr_v kmem_early_next_alloc;
addr_v kmem_early_min_alloc;

extern const void _ld_kmalloc_early_begin;
extern const void _ld_kmalloc_early_end;

static void* _kmalloc_early(size_t size, size_t align, uint32* physical_address, bool track);

#ifdef KMEM_EARLY_TRACK
typedef struct kmem_early_allocator_tracker {
    addr_v caller;
    size_t size_requested;
    size_t size_total;
    
    struct kmem_early_allocator_tracker* next;
    struct kmem_early_allocator_tracker* prev;
} kmem_early_allocator_tracker;
kmem_early_allocator_tracker* first_tracker;
size_t tracker_overhead;

static void _dump_top_allocators(void)
{
    klog(KLOG_LEVEL_DEBUG, "kmem_early: top 3 users:\n");
    
    kmem_early_allocator_tracker* t = first_tracker;
    for (size_t i = 0; i < 3 && t != NULL; t = t->next, i++)
    {
        uint32 ksym_offset;
        const kernel_symbol* ksym = ksym_address_lookup(t->caller, &ksym_offset, KSYM_ALOOKUP_RET);
        
        if (ksym != NULL)
        {
            klog(
                KLOG_LEVEL_DEBUG,
                "kmem_early:   %s+0x%x (%dKiB req, %dKiB total)\n",
                ksym->name,
                ksym_offset,
                t->size_requested / 1024,
                t->size_total / 1024
            );
        }
        else
        {
            klog(
                KLOG_LEVEL_DEBUG,
                "kmem_early:   0x%x (%dKiB req, %dKiB total)\n",
                t->caller,
                t->size_requested / 1024,
                t->size_total / 1024
            );
        }
    }
    
    klog(KLOG_LEVEL_DEBUG, "kmem_early: %dB used for tracking\n", tracker_overhead);
}

static void _move_allocator_tracker(kmem_early_allocator_tracker* t)
{
    if (t->prev != NULL && t->size_total > t->prev->size_total)
    {
        t->prev->next = t->next;
        
        if (t->next)
            t->next->prev = t->prev;
        
        kmem_early_allocator_tracker* pt = t->prev->prev;
        
        while (pt != NULL && t->size_total > pt->size_total)
            pt = pt->prev;
        
        if (pt != NULL)
        {
            t->next = pt->next;
            t->prev = pt;
            pt->next->prev = t;
            pt->next = t;
        }
        else
        {
            t->next = first_tracker;
            t->prev = NULL;
            first_tracker->prev = t;
            first_tracker = t;
        }
    }
}

static void track_allocation(addr_v caller, size_t size_requested, size_t size_total)
{
    kmem_early_allocator_tracker* t;
    
    for (t = first_tracker; t != NULL; t = t->next)
    {
        if (t->caller == caller)
        {
            t->size_requested += size_requested;
            t->size_total += size_total;
            _move_allocator_tracker(t);
            
            return;
        }
    }
    
    t = _kmalloc_early(sizeof(kmem_early_allocator_tracker), __alignof__(kmem_early_allocator_tracker), NULL, false);
    t->caller = caller;
    t->size_requested = size_requested;
    t->size_total = size_total;
    
    if (first_tracker == NULL || t->size_total > first_tracker->size_total)
    {
        t->prev = NULL;
        t->next = first_tracker;
        
        if (first_tracker != NULL)
            first_tracker->prev = t;
        
        first_tracker = t;
    }
    else
    {
        kmem_early_allocator_tracker* pt = first_tracker;
        
        while (pt->next != NULL && t->size_total < pt->next->size_total)
            pt = pt->next;
        
        t->next = pt->next;
        t->prev = pt;
        
        if (pt->next != NULL)
            pt->next->prev = t;
        
        pt->next = t;
    }
}

static void track_allocation_overhead(size_t size_requested, size_t size_total)
{
    tracker_overhead += size_total;
}
#else
#define _dump_top_allocators() ((void)0)
#define track_allocation(c, r, t) ((void)(sizeof(c) + sizeof(r) + sizeof(t)))
#define track_allocation_overhead(r, t) ((void)(sizeof(r) + sizeof(t)))
#endif

static bool verify_mask(size_t mask)
{
    while ((mask & 0x1) != 0x0) mask >>= 1;
    
    return mask == 0x0;
}

static void* _kmalloc_early(size_t size, size_t align, uint32* physical_address, bool track)
{
    uint32 r;
    size_t align_mask = (align < 0x4) ? 0x3 : (align - 1);
    size_t overhead = 0;
    
    assert(init_done && !final_done);
    assert(verify_mask(align_mask));
    assert(kmem_early_next_alloc >= kmem_early_min_alloc);
    
    if ((addr_v)&_ld_kmalloc_early_end - kmem_early_next_alloc < size)
    {
        _dump_top_allocators();
        crash("kmalloc_early out of memory");
    }
    
    r = kmem_early_next_alloc;
    if ((r & align_mask) != 0x0)
    {
        uint32 old_r = r;
        
        r |= align_mask;
        r += 1;
        
        overhead = r - old_r;
    }
    
    assert(r >= kmem_early_next_alloc && (r & align_mask) == 0x0);
    kmem_early_next_alloc = r + size;
    
    if (track)
        track_allocation(get_caller_address(), size, size + overhead);
    else
        track_allocation_overhead(size, size + overhead);
    
    if (physical_address != NULL)
        *physical_address = r - 0xC0000000;
    
    return (void*)r;
}

void* kmalloc_early(size_t size, size_t align, uint32* physical_address)
{
    return _kmalloc_early(size, align, physical_address, true);
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
    
    klog(
        KLOG_LEVEL_DEBUG,
        "kmem_early: final usage of %dKiB/%dKiB\n",
        (kmem_early_next_alloc - kmem_early_min_alloc) / 1024,
        ((addr_v)&_ld_kmalloc_early_end - (addr_v)&_ld_kmalloc_early_begin) / 1024
    );
    _dump_top_allocators();
    
    // The next allocation address should be page-aligned now.
    if ((kmem_early_next_alloc & FRAME_MASK) != 0)
    {
        kmem_early_next_alloc |= FRAME_MASK;
        kmem_early_next_alloc += 1;
    }
}
