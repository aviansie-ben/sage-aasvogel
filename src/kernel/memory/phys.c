#include <memory/phys.h>
#include <memory/page.h>
#include <memory/early.h>
#include <lock/spinlock.h>

#include <core/klog.h>
#include <core/crash.h>

#define EMERG_STACK_SIZE 128

typedef struct
{
    addr_p next_stack_frame;
    addr_p free_frames[(FRAME_SIZE / sizeof(addr_p)) - 1];
} free_frame_stack;

static spinlock free_stack_lock;

static uint32 free_stack_top;
static free_frame_stack free_stack __attribute__((aligned(FRAME_SIZE)));

static uint32 emerg_stack_top;
static addr_p emerg_stack[EMERG_STACK_SIZE];

uint32 kmem_total_frames;
uint32 kmem_free_frames;

static void _push_free_frame(addr_p frame)
{
    kmem_free_frames++;
    
    if (emerg_stack_top != EMERG_STACK_SIZE)
    {
        emerg_stack[emerg_stack_top++] = frame;
    }
    else if (free_stack_top != sizeof(free_stack.free_frames) / sizeof(*free_stack.free_frames))
    {
        free_stack.free_frames[free_stack_top++] = frame;
    }
    else
    {
        addr_p old_free_stack;
        if (!kmem_page_get(&kernel_page_context, (addr_v)&free_stack, &old_free_stack, NULL))
            crash("Free frame stack broken");
        
        if (!kmem_page_global_map((addr_v)&free_stack, PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, true, frame))
            crash("Free frame stack broken!");
        
        free_stack_top = 0;
        free_stack.next_stack_frame = old_free_stack;
    }
}

static addr_p _pop_free_frame(void)
{
    addr_p frame;
    
    if (free_stack_top != 0)
    {
        frame = free_stack.free_frames[--free_stack_top];
        free_stack.free_frames[free_stack_top] = FRAME_NULL;
        
        kmem_free_frames--;
        
        return frame;
    }
    else if (free_stack.next_stack_frame != FRAME_NULL)
    {
        if (!kmem_page_get(&kernel_page_context, (addr_v)&free_stack, &frame, NULL))
            crash("Free frame stack broken");
        
        free_stack_top = sizeof(free_stack.free_frames) / sizeof(*free_stack.free_frames);
        
        if (!kmem_page_global_map((addr_v)&free_stack, PT_ENTRY_WRITEABLE | PT_ENTRY_NO_EXECUTE, true, free_stack.next_stack_frame))
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
    addr_p frame;
    
    while (true)
    {
        frame = _pop_free_frame();
        
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
    addr_p alloc_end = (addr_p)kmem_early_next_alloc;
    
    multiboot_mmap_entry* mmap = (multiboot_mmap_entry*) (param->multiboot->mmap_addr + 0xC0000000);
    multiboot_mmap_entry* mmap_end = (multiboot_mmap_entry*) (param->multiboot->mmap_addr + 0xC0000000 + param->multiboot->mmap_length);
    
    addr_p region_begin;
    addr_p region_end;
    addr_p addr;
    
    if ((alloc_end & 0xfffull) != 0)
    {
        alloc_end |= 0xfffull;
        alloc_end += 1;
    }
    
    while (mmap != mmap_end)
    {
        region_begin = mmap->base_addr;
        region_end = mmap->base_addr + mmap->length;
        
        region_end &= ~0xfffull;
        
        if ((region_begin & 0xfffull) != 0)
        {
            region_begin |= 0xfffull;
            region_begin += 1;
        }
        
        kmem_total_frames += (uint32)((region_end - region_begin) / FRAME_SIZE);
        
        if (mmap->type == 1)
        {
            if (region_end > alloc_end)
            {
                if (region_begin < alloc_end)
                {
#ifdef MMAP_DEBUG
                    klog(KLOG_LEVEL_DEBUG, "MMAP - 0x%lx > 0x%lx - KERNEL RESERVED\n", region_begin, alloc_end);
                    klog(KLOG_LEVEL_DEBUG, "MMAP - 0x%lx > 0x%lx - FREE\n", alloc_end, region_end);
#endif
                    region_begin = alloc_end;
                }
#ifdef MMAP_DEBUG
                else
                {
                    klog(KLOG_LEVEL_DEBUG, "MMAP - 0x%lx > 0x%lx - FREE\n", region_begin, region_end);
                }
#endif
                
                for (addr = region_begin; addr != region_end; addr += 0x1000)
                {
                    _push_free_frame(addr);
                }
            }
#ifdef MMAP_DEBUG
            else
            {
                klog(KLOG_LEVEL_DEBUG, "MMAP - 0x%lx > 0x%lx - KERNEL RESERVED\n", region_begin, region_end);
            }
#endif
        }
#ifdef MMAP_DEBUG
        else
        {
            klog(KLOG_LEVEL_DEBUG, "MMAP - 0x%lx > 0x%lx - SYSTEM RESERVED (TYPE %d)\n", region_begin, region_end, mmap->type);
        }
#endif
        
        mmap = (multiboot_mmap_entry*) ((uint32) mmap + mmap->size + sizeof(mmap->size));
    }
}

void kmem_phys_init(const boot_param* param)
{
    free_stack.next_stack_frame = FRAME_NULL;
    free_stack_top = 0;
    
    _push_unallocated(param);
    
    klog(KLOG_LEVEL_INFO, "Found %dKiB of memory, with %dKiB free.\n", kmem_total_frames * 4, kmem_free_frames * 4);
}

addr_p kmem_frame_alloc(frame_alloc_flags flags)
{
    addr_p frame;
    
    spinlock_acquire(&free_stack_lock);
    frame = _alloc_frame(flags);
    spinlock_release(&free_stack_lock);
    
    return frame;
}

void kmem_frame_free(addr_p frame)
{
    spinlock_acquire(&free_stack_lock);
    _push_free_frame(frame);
    spinlock_release(&free_stack_lock);
}

size_t kmem_frame_alloc_many(addr_p* frames, size_t num_frames, frame_alloc_flags flags)
{
    addr_p frame;
    size_t i;
    
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
    spinlock_acquire(&free_stack_lock);
    while (num_frames-- != 0)
    {
        _push_free_frame(*frames++);
    }
    spinlock_release(&free_stack_lock);
}
