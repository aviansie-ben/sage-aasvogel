#include <core/cpuid.h>
#include <core/msr.h>
#include <core/idt.h>
#include <memory/early.h>
#include <memory/phys.h>
#include <memory/page.h>
#include <memory/virt.h>
#include <assert.h>

#define ALLOW_PAGE_H_DIRECT
#include <memory/page_pae.h>

extern const void _ld_text_begin;
extern const void _ld_text_end;

extern const void _ld_rodata_begin;
extern const void _ld_rodata_end;

extern const void _ld_data_begin;
extern const void _ld_data_end;

extern const void _ld_bss_begin;
extern const void _ld_bss_end;

page_context kernel_page_context;
page_context* active_page_context;

addr_v kmem_page_resv_end;

bool kmem_page_pae_enabled = false;
bool kmem_page_pge_enabled = false;

static void _init_map_range(addr_v start, addr_v end, uint64 flags)
{
    bool result;
    
    flags |= PT_ENTRY_PRESENT;
    if (!kmem_page_pge_enabled) flags &= (uint64)~PT_ENTRY_GLOBAL;
    
    for (start &= (addr_v)~FRAME_MASK; start < end; start += FRAME_SIZE)
    {
        if (kmem_page_pae_enabled) result = kmem_page_pae_set(&kernel_page_context, start, (addr_p)(start - KERNEL_VIRTUAL_ADDRESS_BEGIN), flags);
        else crash("Legacy paging not implemented!");
        
        if (!result)
            crash("Failed to map initial kernel memory!");
    }
}

static void _init_map_kmalloc_early(uint64 flags)
{
    addr_v addr;
    bool result;
    
    flags |= PT_ENTRY_PRESENT;
    if (!kmem_page_pge_enabled) flags &= (uint64)~PT_ENTRY_GLOBAL;
    
    for (addr = kmem_early_min_alloc & (addr_v)~FRAME_MASK; addr < kmem_early_next_alloc; addr += FRAME_SIZE)
    {
        if (kmem_page_pae_enabled) result = kmem_page_pae_set(&kernel_page_context, addr, (addr_p)(addr - KERNEL_VIRTUAL_ADDRESS_BEGIN), flags);
        else crash("Legacy paging not implemented!");
        
        if (!result)
            crash("Failed to map initial kernel memory!");
    }
}

void kmem_page_init(const boot_param* param)
{
    uint32 cr4;
    
    // Determine whether PAE has been enabled. If PAE is supported and was
    // requested, then the pre-initialization boot code will have enabled it
    // for us.
    asm volatile ("mov %%cr4, %0" : "=r" (cr4));
    kmem_page_pae_enabled = ((cr4 & (1 << 5)) != 0);
    
    // Determine whether the CPU supports PGE and enable it if it is supported
    // and was requested.
    kmem_page_pge_enabled = !cmdline_get_bool(param, "no_pge") && cpuid_supports_feature_edx(CPUID_FEATURE_EDX_PGE);
    if (kmem_page_pge_enabled)
    {
        cr4 |= 0x80;
        asm volatile ("mov %0, %%cr4" : : "r" (cr4));
    }
    
    // Register the page fault interrupt to crash the system
    idt_register_isr_handler(0xe, do_crash_pagefault);
    
    // Perform stage 1 initialization
    if (kmem_page_pae_enabled) kmem_page_pae_init(param);
    else crash("Legacy paging not implemented!");
    
    // Initialize the kernel page context
    spinlock_init(&kernel_page_context.lock);
    kernel_page_context.prev = NULL;
    kernel_page_context.next = NULL;
    
    if (kmem_page_pae_enabled) kmem_page_pae_context_create(&kernel_page_context, true);
    
    // Map all the necessary regions of memory
    _init_map_range(KERNEL_VIRTUAL_ADDRESS_BEGIN, KERNEL_VIRTUAL_ADDRESS_BEGIN + 0x100000, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    _init_map_range((addr_v)&_ld_text_begin, (addr_v)&_ld_text_end, PT_ENTRY_GLOBAL);
    _init_map_range((addr_v)&_ld_rodata_begin, (addr_v)&_ld_rodata_end, PT_ENTRY_NO_EXECUTE | PT_ENTRY_GLOBAL);
    _init_map_range((addr_v)&_ld_data_begin, (addr_v)&_ld_data_end, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    _init_map_range((addr_v)&_ld_bss_begin, (addr_v)&_ld_bss_end, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    _init_map_kmalloc_early(PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    
    // Perform stage 2 initialization
    if (kmem_page_pae_enabled) kmem_page_pae_init2(param);
    else crash("Legacy paging not implemented!");
    
    // Finally, switch into the new paging context
    kmem_page_context_switch(&kernel_page_context);
}

bool kmem_page_context_create(page_context* c)
{
    // Initialize and allocate memory for the new paging context
    spinlock_init(&c->lock);
    if (kmem_page_pae_enabled)
    {
        if (!kmem_page_pae_context_create(c, false))
            return false;
    }
    else
    {
        crash("Legacy paging not implemented!");
    }
    
    // Insert the new paging context into the list of active paging contexts
    spinlock_acquire(&kernel_page_context.lock);
    c->next = kernel_page_context.next;
    c->prev = &kernel_page_context;
    if (c->next != NULL) c->next->prev = c;
    kernel_page_context.next = c;
    spinlock_release(&kernel_page_context.lock);
    
    return false;
}

void kmem_page_context_destroy(page_context* c)
{
    if (c == &kernel_page_context)
        crash("Attempt to destroy the kernel page context!");
    
    // Remove the paging context from the list of active paging contexts
    spinlock_acquire(&kernel_page_context.lock);
    c->prev->next = c->next;
    if (c->next != NULL) c->next->prev = c->prev;
    spinlock_release(&kernel_page_context.lock);
    
    // Release all memory associated with the paging context
    if (kmem_page_pae_enabled) kmem_page_pae_context_destroy(c);
    else crash("Legacy paging not implemented!");
}

void kmem_page_context_switch(page_context* c)
{
    active_page_context = c;
    asm volatile ("mov %0, %%cr3" : : "r" (c->physical_address));
}

void* kmem_page_global_alloc(uint64 page_flags, frame_alloc_flags alloc_flags, uint32 num_pages)
{
    addr_v page;
    addr_p frames[num_pages];
    size_t frames_n;
    size_t i, j;
    
    frames_n = kmem_frame_alloc_many(frames, num_pages, alloc_flags);
    
    if (frames_n < num_pages)
    {
        kmem_frame_free_many(frames, frames_n);
        return NULL;
    }
    
    page = (addr_v) kmem_virt_alloc(num_pages);
    
    if (page == 0)
    {
        kmem_frame_free_many(frames, frames_n);
        return NULL;
    }
    
    for (i = 0; i < num_pages; i++)
    {
        if (!kmem_page_global_map(page + (i * FRAME_SIZE), page_flags, false, frames[i]))
        {
            for (j = 0; j < i; j++)
                kmem_page_global_unmap(page + (i * FRAME_SIZE), false);
            
            kmem_frame_free_many(frames, frames_n);
            kmem_virt_free((void*) page, num_pages);
            
            return NULL;
        }
    }
    
    kmem_page_flush_region(page, num_pages);
    return (void*)page;
}

void kmem_page_global_free(void* addr, uint32 num_pages)
{
    addr_p frames[num_pages];
    size_t i;
    
    for (i = 0; i < num_pages; i++)
    {
        if (!kmem_page_global_get((addr_v)addr + (i * FRAME_SIZE), &frames[i], NULL))
            crash("Attempt to free a page that wasn't allocated!");
        kmem_page_global_unmap((addr_v)addr + (i * FRAME_SIZE), false);
    }
    
    kmem_page_flush_region((addr_v)addr, num_pages);
    kmem_virt_free(addr, num_pages);
    
    for (i = 0; i < num_pages; i++)
        kmem_frame_free(frames[i]);
}

bool _kmem_page_get(page_context* c, addr_v virtual_address, addr_p* physical_address, uint64* flags)
{
    if (kmem_page_pae_enabled) return kmem_page_pae_get(c, virtual_address, physical_address, flags);
    else crash("Legacy paging not implemented!");
}

bool _kmem_page_map(page_context* c, addr_v virtual_address, uint64 flags, bool flush, addr_p frame)
{
    bool result;
    
    if (!kmem_page_pge_enabled) flags &= (uint64)~PT_ENTRY_GLOBAL;
    
    if (kmem_page_pae_enabled) result = kmem_page_pae_set(c, virtual_address, frame, flags | PT_ENTRY_PRESENT);
    else crash("Legacy paging not implemented!");
    
    if (result && flush) kmem_page_flush_one(virtual_address);
    return result;
}

void _kmem_page_unmap(page_context* c, addr_v virtual_address, bool flush)
{
    if (kmem_page_pae_enabled)
    {
        if (kmem_page_pae_get(c, virtual_address, NULL, NULL))
            kmem_page_pae_set(c, virtual_address, 0, 0);
    }
    else
    {
        crash("Legacy paging not implemented!");
    }
    
    if (flush) kmem_page_flush_one(virtual_address);
}

bool _kmem_page_global_get(addr_v virtual_address, addr_p* physical_address, uint64* flags)
{
    return _kmem_page_get(&kernel_page_context, virtual_address, physical_address, flags);
}

bool _kmem_page_global_map(addr_v virtual_address, uint64 flags, bool flush, addr_p frame)
{
    if (kmem_page_pae_enabled)
        return _kmem_page_map(&kernel_page_context, virtual_address, flags, flush, frame);
    
    crash("Legacy paging not implemented!");
}

void _kmem_page_global_unmap(addr_v virtual_address, bool flush)
{
    if (kmem_page_pae_enabled)
    {
        _kmem_page_unmap(&kernel_page_context, virtual_address, flush);
        return;
    }
    
    crash("Legacy paging not implemented!");
}

bool kmem_page_get(page_context* c, addr_v virtual_address, addr_p* physical_address, uint64* flags)
{
    bool result;
    
    spinlock_acquire(&c->lock);
    result = _kmem_page_get(c, virtual_address, physical_address, flags);
    spinlock_release(&c->lock);
    
    return result;
}

bool kmem_page_map(page_context* c, addr_v virtual_address, uint64 flags, bool flush, addr_p frame)
{
    bool result;
    
    spinlock_acquire(&c->lock);
    result = _kmem_page_map(c, virtual_address, flags, flush, frame);
    spinlock_release(&c->lock);
    
    return result;
}

void kmem_page_unmap(page_context* c, addr_v virtual_address, bool flush)
{
    spinlock_acquire(&c->lock);
    _kmem_page_unmap(c, virtual_address, flush);
    spinlock_release(&c->lock);
}

bool kmem_page_global_get(addr_v virtual_address, addr_p* physical_address, uint64* flags)
{
    bool result;
    
    spinlock_acquire(&kernel_page_context.lock);
    result = _kmem_page_global_get(virtual_address, physical_address, flags);
    spinlock_release(&kernel_page_context.lock);
    
    return result;
}

bool kmem_page_global_map(addr_v virtual_address, uint64 flags, bool flush, addr_p frame)
{
    bool result;
    
    spinlock_acquire(&kernel_page_context.lock);
    result = _kmem_page_global_map(virtual_address, flags, flush, frame);
    spinlock_release(&kernel_page_context.lock);
    
    return result;
}

void kmem_page_global_unmap(addr_v virtual_address, bool flush)
{
    spinlock_acquire(&kernel_page_context.lock);
    _kmem_page_global_unmap(virtual_address, flush);
    spinlock_release(&kernel_page_context.lock);
}

void kmem_page_flush_one(addr_v virtual_address)
{
    // If the CPU supports PGE, we can flush a single TLB entry using the INVLPG
    // instruction. Otherwise, we must flush the entire TLB.
    if (kmem_page_pge_enabled)
        asm volatile ("invlpg (%0)" : : "r" (virtual_address));
    else
        kmem_page_flush_all();
}

void kmem_page_flush_region(addr_v virtual_address, uint32 num_pages)
{
    if (kmem_page_pge_enabled)
    {
        while (num_pages != 0)
        {
            asm volatile ("invlpg (%0)" : : "r" (virtual_address));
            
            virtual_address += FRAME_SIZE;
            num_pages--;
        }
    }
    else
    {
        kmem_page_flush_all();
    }
}

void kmem_page_flush_all(void)
{
    uint32 cr;
    
    if (kmem_page_pge_enabled)
    {
        // Enabling and disabling PGE will cause the entire TLB to be flushed
        // (including any pages marked as global)
        asm volatile ("mov %%cr4, %0" : "=r" (cr));
        asm volatile ("mov %0, %%cr4" : : "r" (cr & ~0x80u));
        asm volatile ("mov %0, %%cr4" : : "r" (cr));
    }
    else
    {
        // Re-writing CR3 will cause the entire TLB to be flushed
        asm volatile ("mov %%cr3, %0" : "=r" (cr));
        asm volatile ("mov %0, %%cr3" : : "r" (cr));
    }
}

void kmem_enable_write_protect(void)
{
    uint32 cr0;
    
    asm volatile ("mov %%cr0, %0" : "=r" (cr0));
    asm volatile ("mov %0, %%cr0" : : "r" (cr0 | 0x10000));
}

void kmem_disable_write_protect(void)
{
    uint32 cr0;
    
    asm volatile ("mov %%cr0, %0" : "=r" (cr0));
    asm volatile ("mov %0, %%cr0" : : "r" (cr0 & ~0x10000u));
}
