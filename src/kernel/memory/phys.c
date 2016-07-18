#include <memory/phys.h>
#include <memory/page.h>
#include <memory/early.h>
#include <lock/spinlock.h>

#include <core/klog.h>
#include <core/crash.h>
#include <assert.h>

#define EMERG_STACK_SIZE 128
#define FRAMES_PER_STACK_FRAME ((FRAME_SIZE / sizeof(addr_p)) - 1)

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
    
    if (frame >= (1ull << 32))
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
        if ((flags & FA_32BIT) == 0 && high_stack_enabled)
            frame = _pop_free_frame(&high_stack_top, &high_stack);
        
        if (frame == FRAME_NULL)
            frame = _pop_free_frame(&free_stack_top, &free_stack);
        
        if (frame == FRAME_NULL && (flags & FA_EMERG) != 0)
        {
            frame = _pop_emerg_frame();
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

static void _push_unallocated(const boot_param* param)
{
    bool high_warn = false;
    addr_p alloc_end = (addr_p)(kmem_early_next_alloc - 0xC0000000);
    
    const boot_param_mmap_region* region;
    
    addr_p region_begin;
    addr_p region_end;
    addr_p addr;
    
    if ((alloc_end & 0xfffull) != 0)
    {
        alloc_end |= 0xfffull;
        alloc_end += 1;
    }
    
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
            if (region_end > alloc_end)
            {
                if (region_begin < alloc_end)
                {
#ifdef MMAP_DEBUG
                    klog(KLOG_LEVEL_DEBUG, "mmap: 0x%016lx -> 0x%016lx - KERNEL RESERVED\n", region_begin, alloc_end);
                    klog(KLOG_LEVEL_DEBUG, "mmap: 0x%016lx -> 0x%016lx - FREE\n", alloc_end, region_end);
#endif
                    region_begin = alloc_end;
                }
#ifdef MMAP_DEBUG
                else
                {
                    klog(KLOG_LEVEL_DEBUG, "mmap: 0x%016lx -> 0x%016lx - FREE\n", region_begin, region_end);
                }
#endif
                
                for (addr = region_begin; addr != region_end; addr += 0x1000)
                {
                    if (addr >= (1ull << 32) && !high_stack_enabled)
                    {
                        if (!high_warn)
                        {
                            klog(KLOG_LEVEL_WARN, "Memory beyond 4GiB detected, but unusable since PAE is disabled\n");
                            high_warn = true;
                        }
                        
                        kmem_total_frames -= (uint32)((region_end - addr) / FRAME_SIZE);
                        break;
                    }
                    
                    _push_free_frame(addr);
                }
            }
#ifdef MMAP_DEBUG
            else
            {
                klog(KLOG_LEVEL_DEBUG, "mmap: 0x%016lx -> 0x%016lx - KERNEL RESERVED\n", region_begin, region_end);
            }
#endif
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
    
    _push_unallocated(param);
    
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
