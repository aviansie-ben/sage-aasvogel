#include <memory/phys.h>
#include <memory/page.h>
#include <memory/early.h>
#include <lock/spinlock.h>

#include <core/klog.h>
#include <core/crash.h>
#include <assert.h>

#define EMERG_STACK_SIZE 128
#define FRAMES_PER_STACK_FRAME ((FRAME_SIZE / sizeof(addr_p)) - 1)

extern const void _ld_kernel_begin;
extern const void _ld_kernel_end;
extern const void _ld_kmalloc_early_begin;

typedef struct
{
    addr_p start_address;
    addr_p end_address;
    
    const char* name;
} kernel_resv_region;

typedef struct
{
    addr_p next_stack_frame;
    addr_p free_frames[(FRAME_SIZE / sizeof(addr_p)) - 1];
} free_frame_stack;

static bool init_done;
static spinlock free_stack_lock;

static bool high_stack_enabled;
static uint32 high_stack_top;
static volatile free_frame_stack high_stack __attribute__((aligned(FRAME_SIZE)));

static uint32 low_stack_top;
static volatile free_frame_stack low_stack __attribute__((aligned(FRAME_SIZE)));

static uint32 free_stack_top;
static volatile free_frame_stack free_stack __attribute__((aligned(FRAME_SIZE)));

static uint32 emerg_stack_top;
static addr_p emerg_stack[EMERG_STACK_SIZE];

uint32 kmem_total_frames;
uint32 kmem_free_frames;

static void _push_free_frame_stack(uint32* stack_top, volatile free_frame_stack* stack, addr_p frame)
{
    if (*stack_top != FRAMES_PER_STACK_FRAME)
    {
        stack->free_frames[(*stack_top)++] = frame;
    }
    else
    {
        addr_p old_stack;
        if (!_kmem_page_global_get((addr_v)stack, &old_stack, NULL))
            crash("Free frame stack broken");
        
        if (!_kmem_page_global_map((addr_v)stack, PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, true, frame))
            crash("Free frame stack broken!");
        
        *stack_top = 0;
        stack->next_stack_frame = old_stack;
    }
}

static void _push_free_frame(addr_p frame)
{
    kmem_free_frames++;
    
    if (frame < (1ull << 20))
    {
        _push_free_frame_stack(&low_stack_top, &low_stack, frame);
    }
    else if (frame >= (1ull << 32))
    {
        assert(high_stack_enabled);
        _push_free_frame_stack(&high_stack_top, &high_stack, frame);
    }
    else
    {
        if (emerg_stack_top != EMERG_STACK_SIZE)
        {
            emerg_stack[emerg_stack_top++] = frame;
        }
        else
        {
            _push_free_frame_stack(&free_stack_top, &free_stack, frame);
        }
    }
}

static addr_p _pop_free_frame(uint32* stack_top, volatile free_frame_stack* stack)
{
    addr_p frame;
    
    if (*stack_top != 0)
    {
        frame = stack->free_frames[--(*stack_top)];
        stack->free_frames[*stack_top] = FRAME_NULL;
        
        kmem_free_frames--;
        
        return frame;
    }
    else if (stack->next_stack_frame != FRAME_NULL)
    {
        if (!_kmem_page_global_get((addr_v)stack, &frame, NULL))
            crash("Free frame stack broken");
        
        *stack_top = FRAMES_PER_STACK_FRAME;
        
        if (!_kmem_page_global_map((addr_v)stack, PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, true, stack->next_stack_frame))
            crash("Free frame stack broken!");
        
        kmem_free_frames--;
        
        return frame;
    }
    else
    {
        return FRAME_NULL;
    }
}

static addr_p _pop_emerg_frame(void)
{
    addr_p frame;
    
    if (emerg_stack_top != 0)
    {
        frame = emerg_stack[--emerg_stack_top];
        emerg_stack[emerg_stack_top] = FRAME_NULL;
        
        kmem_free_frames--;
        
        return frame;
    }
    else
    {
        return FRAME_NULL;
    }
}

static addr_p _alloc_frame(frame_alloc_flags flags)
{
    addr_p frame = FRAME_NULL;
    
    while (true)
    {
        if ((flags & FA_LOW_MEM) != 0)
        {
            frame = _pop_free_frame(&low_stack_top, &low_stack);
        }
        else
        {
            if ((flags & FA_32BIT) == 0 && high_stack_enabled)
                frame = _pop_free_frame(&high_stack_top, &high_stack);
            
            if (frame == FRAME_NULL)
                frame = _pop_free_frame(&free_stack_top, &free_stack);
            
            if (frame == FRAME_NULL && (flags & FA_EMERG) != 0)
            {
                frame = _pop_emerg_frame();
                
                if (frame == FRAME_NULL)
                    frame = _pop_free_frame(&low_stack_top, &low_stack);
            }
        }
        
        if (frame == FRAME_NULL && (flags & FA_WAIT) != 0)
        {
            spinlock_release(&free_stack_lock);
            // TODO Wait for a frame to be freed
            spinlock_acquire(&free_stack_lock);
            
            continue;
        }
        
        return frame;
    }
}

static void _push_region_frames(addr_p region_start, addr_p region_end, bool* high_warn)
{
    assert((region_start & FRAME_OFFSET_MASK) == 0);
    assert((region_end & FRAME_OFFSET_MASK) == 0);
    
    if (region_end > (1ull << 32) && !high_stack_enabled)
    {
        if (!*high_warn)
        {
            klog(KLOG_LEVEL_WARN, "Memory beyond 4GiB detected, but unusable since PAE is disabled\n");
            *high_warn = true;
        }
        
        region_end = (1ull << 32);
    }
    
    for (addr_p addr = region_start; addr != region_end; addr += FRAME_SIZE)
        _push_free_frame(addr);
}

static void _push_region_non_reserved_frames(addr_p region_start, addr_p region_end, bool* high_warn, size_t num_resv_regions, const kernel_resv_region* resv_regions)
{
    assert((region_start & FRAME_OFFSET_MASK) == 0);
    assert((region_end & FRAME_OFFSET_MASK) == 0);
    
    for (size_t i = 0; i < num_resv_regions; i++)
    {
        const kernel_resv_region* rr = &resv_regions[i];
        
        if (rr->end_address > region_start && rr->end_address < region_end)
        {
            if (rr->start_address > region_start)
                _push_region_non_reserved_frames(region_start, rr->start_address, high_warn, num_resv_regions, resv_regions);
            
#ifdef MMAP_DEBUG
            klog(
                KLOG_LEVEL_DEBUG,
                "mmap: 0x%016lx -> 0x%016lx - %s\n",
                rr->start_address > region_start ? rr->start_address : region_start,
                rr->end_address,
                rr->name
            );
#endif
            
            _push_region_non_reserved_frames(rr->end_address, region_end, high_warn, num_resv_regions, resv_regions);
            return;
        }
        else if (rr->start_address > region_start && rr->start_address < region_end)
        {
            _push_region_non_reserved_frames(region_start, rr->end_address, high_warn, num_resv_regions, resv_regions);
            
#ifdef MMAP_DEBUG
            klog(
                KLOG_LEVEL_DEBUG,
                "mmap: 0x%016lx -> 0x%016lx - %s\n",
                rr->start_address,
                rr->end_address < region_end ? rr->end_address : region_end,
                rr->name
            );
#endif
            
            if (rr->end_address < region_end)
                _push_region_non_reserved_frames(rr->end_address, region_end, high_warn, num_resv_regions, resv_regions);
            
            return;
        }
        else if (rr->start_address <= region_start && rr->end_address >= region_end)
        {
#ifdef MMAP_DEBUG
            klog(
                KLOG_LEVEL_DEBUG,
                "mmap: 0x%016lx -> 0x%016lx - %s\n",
                region_start,
                region_end,
                rr->name
            );
#endif
            return;
        }
    }
    
#ifdef MMAP_DEBUG
    klog(
        KLOG_LEVEL_DEBUG,
        "mmap: 0x%016lx -> 0x%016lx - FREE\n",
        region_start,
        region_end
    );
#endif
    
    _push_region_frames(region_start, region_end, high_warn);
}

static void _push_unallocated(const boot_param* param, size_t num_resv_regions, const kernel_resv_region* resv_regions)
{
    bool high_warn = false;
    
    const boot_param_mmap_region* region;
    
    addr_p region_begin;
    addr_p region_end;
    
    for (size_t i = 0; i < param->num_mmap_regions; i++)
    {
        region = &param->mmap_regions[i];
        
        region_begin = region->start_address;
        region_end = region->end_address;
        
        region_end &= ~0xfffull;
        
        if ((region_begin & 0xfffull) != 0)
        {
            region_begin |= 0xfffull;
            region_begin += 1;
        }
        
        if (region_begin == region_end) continue;
        
        kmem_total_frames += (uint32)((region_end - region_begin) / FRAME_SIZE);
        
        if (region->type == 1)
        {
            _push_region_non_reserved_frames(region_begin, region_end, &high_warn, num_resv_regions, resv_regions);
        }
#ifdef MMAP_DEBUG
        else
        {
            klog(KLOG_LEVEL_DEBUG, "mmap: 0x%016lx -> 0x%016lx - SYSTEM RESERVED (%d)\n", region_begin, region_end, region->type);
        }
#endif
    }
}

void kmem_phys_init(const boot_param* param)
{
    free_stack.next_stack_frame = FRAME_NULL;
    free_stack_top = 0;
    
    high_stack_enabled = kmem_page_pae_enabled;
    high_stack.next_stack_frame = FRAME_NULL;
    high_stack_top = 0;
    
    kernel_resv_region resv_regions[] = {
        { 0x0, 0x1000, "NULL FRAME" },
        { (addr_p)(uint32) &_ld_kernel_begin, (addr_p)(uint32) &_ld_kernel_end, "KERNEL BIN" },
        { (addr_p)(uint32) &_ld_kmalloc_early_begin - 0xC0000000, (addr_p) kmem_early_next_alloc - 0xC0000000, "KMALLOC_EARLY" }
    };
    
    _push_unallocated(param, sizeof(resv_regions) / sizeof(*resv_regions), resv_regions);
    
    klog(KLOG_LEVEL_INFO, "Found %dKiB of memory, with %dKiB free.\n", kmem_total_frames * 4, kmem_free_frames * 4);
    init_done = true;
}

addr_p kmem_frame_alloc(frame_alloc_flags flags)
{
    addr_p frame;
    
    assert(init_done);
    
    spinlock_acquire(&free_stack_lock);
    frame = _alloc_frame(flags);
    spinlock_release(&free_stack_lock);
    
    return frame;
}

void kmem_frame_free(addr_p frame)
{
    assert(init_done);
    
    spinlock_acquire(&free_stack_lock);
    _push_free_frame(frame);
    spinlock_release(&free_stack_lock);
}

size_t kmem_frame_alloc_many(addr_p* frames, size_t num_frames, frame_alloc_flags flags)
{
    addr_p frame;
    size_t i;
    
    assert(init_done);
    
    spinlock_acquire(&free_stack_lock);
    
    for (i = 0; i < num_frames; i++)
    {
        frame = _alloc_frame(flags);
        
        if (frame != FRAME_NULL)
        {
            *frames++ = frame;
        }
        else
        {
            break;
        }
    }
    
    spinlock_release(&free_stack_lock);
    return i;
}

void kmem_frame_free_many(const addr_p* frames, size_t num_frames)
{
    assert(init_done);
    
    spinlock_acquire(&free_stack_lock);
    while (num_frames-- != 0)
    {
        _push_free_frame(*frames++);
    }
    spinlock_release(&free_stack_lock);
}
