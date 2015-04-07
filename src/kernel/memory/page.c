#include <core/cpuid.h>
#include <memory/early.h>
#include <memory/phys.h>
#include <memory/page.h>
#include <assert.h>

page_context kernel_page_context;
page_context* active_page_context;

static bool init_done = false;
static bool use_pae = false;
static bool use_pge = false;

static void kmem_page_init_pae(multiboot_info* multiboot)
{
    page_dir_ptr_tab* pdpt;
    page_dir_pae* pd;
    uint32 phys;
    uint32 i;
    
    spinlock_init(&kernel_page_context.lock);
    
    pdpt = kernel_page_context.pae_pdpt = kmalloc_early(sizeof(page_dir_ptr_tab), __alignof__(page_dir_ptr_tab), &phys);
    kernel_page_context.physical_address = phys;
    pdpt->page_dir_phys[0] = pdpt->page_dir_phys[1] = pdpt->page_dir_phys[2] = 0;
    pdpt->page_dir_virt[0] = pdpt->page_dir_virt[1] = pdpt->page_dir_virt[2] = NULL;
    for (i = 0; i < (sizeof(pdpt->page_table_virt) / sizeof(*pdpt->page_table_virt)); i++)
        pdpt->page_table_virt[i] = NULL;
    
    pd = pdpt->page_dir_virt[3] = kmalloc_early(sizeof(page_dir_pae), __alignof__(page_dir_pae), &phys);
    pdpt->page_dir_phys[3] = phys;
    for (i = 0; i < (sizeof(pd->page_table_phys) / sizeof(*pd->page_table_phys)); i++)
        pd->page_table_phys[i] = 0;
}

static void kmem_page_init_legacy(multiboot_info* multiboot)
{
    crash("Legacy paging support not implemented yet!");
}

void kmem_page_init(multiboot_info* multiboot)
{
    uint32 cr4;
    
    assert(!init_done);
    
    use_pge = cpuid_supports_feature_edx(CPUID_FEATURE_EDX_PGE);
    
    asm volatile ("mov %%cr4, %0" : "=r" (cr4));
    use_pae = ((cr4 & (1 << 5)) != 0);
    
    // If the CPU supports PGE, enable it.
    if (use_pge)
    {
        cr4 |= 0x80;
        asm volatile ("mov %0, %%cr4" : : "r" (cr4));
    }
    
    if (use_pae)
        kmem_page_init_pae(multiboot);
    else
        kmem_page_init_legacy(multiboot);
    
    // TODO: Make mappings on the new page context
    
    // Now that we're done setting up the new page context, we can switch into
    // it.
    // kmem_page_context_switch(&kernel_page_context);
}

void kmem_page_context_switch(page_context* c)
{
    active_page_context = c;
    asm volatile ("mov %0, %%cr3" : : "r" (c->physical_address));
}

static mem_block* kmem_page_get_mapping_pae(page_context* c, uint32 virtual_address)
{
    uint32 pto, po;
    page_table_pae* t;
    
    pto = (virtual_address & 0xffe00000) >> 21;
    po = (virtual_address & 0x001ff000) >> 12;
    
    t = c->pae_pdpt->page_table_virt[pto];
    
    if (t == NULL || (t->page_phys[po] & PT_ENTRY_PRESENT) == 0)
        return NULL;
    else
        return kmem_block_find(t->page_phys[po] & PAGE_PHYSICAL_ADDRESS_MASK_64);
}

static mem_block* kmem_page_get_mapping_legacy(page_context* c, uint32 virtual_address)
{
    crash("Legacy paging support not implemented yet!");
}

mem_block* kmem_page_get_mapping(page_context* c, uint32 virtual_address)
{
    mem_block* b;
    
    spinlock_acquire(&c->lock);
    
    if (use_pae)
        b = kmem_page_get_mapping_pae(c, virtual_address);
    else
        b = kmem_page_get_mapping_legacy(c, virtual_address);
    
    spinlock_release(&c->lock);
    return b;
}

static page_table_pae* kmem_page_table_create_pae(page_context* c, uint32 table_number, uint64 flags)
{
    uint32 pdo, pto;
    page_dir_pae* pd;
    page_table_pae* pt;
    uint32 phys;
    uint32 i;
    
    pdo = (table_number & 0x600) >> 9;
    pto = table_number & 0x1ff;
    
    pd = c->pae_pdpt->page_dir_virt[pdo];
    
    if (pd == NULL)
    {
        // TODO: Use proper memory allocation once it's available
        c->pae_pdpt->page_dir_virt[pdo] = pd = kmalloc_early(sizeof(page_dir_pae), __alignof__(page_dir_pae), &phys);
        c->pae_pdpt->page_dir_phys[pdo] = phys | PDPT_ENTRY_PRESENT;
        
        for (i = 0; i < sizeof(pd->page_table_phys) / sizeof(*pd->page_table_phys); i++)
            pd->page_table_phys[i] = 0;
    }
    
    // TODO: Use proper memory allocation once it's available
    c->pae_pdpt->page_table_virt[table_number] = pt = kmalloc_early(sizeof(page_table_pae), __alignof__(page_table_pae), &phys);
    pd->page_table_phys[pto] = phys | flags | PD_ENTRY_PRESENT;
    
    for (i = 0; i < sizeof(pt->page_phys) / sizeof(*pt->page_phys); i++)
        pt->page_phys[i] = 0;
    
    return pt;
}

static void kmem_page_map_pae(page_context* c, uint32 virtual_address, uint64 flags, mem_block* block)
{
    page_table_pae* pt;
    uint32 pto, po;
    
    pto = (virtual_address & 0xffe00000) >> 21;
    po = (virtual_address & 0x001ff000) >> 12;
    
    pt = c->pae_pdpt->page_table_virt[pto];
    if (pt == NULL)
        pt = kmem_page_table_create_pae(c, pto, (virtual_address >= KERNEL_VIRTUAL_ADDRESS_BEGIN) ? PD_ENTRY_DEF_KERNEL : PD_ENTRY_DEF_USER);
    
    pt->page_phys[po] = kmem_block_address(block) | flags | PT_ENTRY_PRESENT;
}

static void kmem_page_map_legacy(page_context* c, uint32 virtual_address, uint32 flags, mem_block* block)
{
    crash("Legacy paging support not implemented yet!");
}

void kmem_page_map(page_context* c, uint32 virtual_address, uint64 flags, bool flush, mem_block* block)
{
    spinlock_acquire(&c->lock);
    
    if (use_pae)
        kmem_page_map_pae(c, virtual_address, flags, block);
    else
        kmem_page_map_legacy(c, virtual_address, (uint32)flags, block);
    
    if (flush)
        kmem_page_flush_one(virtual_address);
    
    spinlock_release(&c->lock);
}

static void kmem_page_unmap_pae(page_context* c, uint32 virtual_address)
{
    page_table_pae* pt;
    uint32 pto, po;
    
    pto = (virtual_address & 0xffe00000) >> 21;
    po = (virtual_address & 0x001ff000) >> 12;
    
    pt = c->pae_pdpt->page_table_virt[pto];
    
    if (pt != NULL)
        pt->page_phys[po] = 0;
}

static void kmem_page_unmap_legacy(page_context* c, uint32 virtual_address)
{
    crash("Legacy paging support not implemented yet!");
}

void kmem_page_unmap(page_context* c, uint32 virtual_address, bool flush)
{
    spinlock_acquire(&c->lock);
    
    if (use_pae)
        kmem_page_unmap_pae(c, virtual_address);
    else
        kmem_page_unmap_legacy(c, virtual_address);
    
    if (flush)
        kmem_page_flush_one(virtual_address);
    
    spinlock_release(&c->lock);
}

void kmem_page_flush_one(uint32 virtual_address)
{
    // If the CPU supports PGE, we can flush a single TLB entry using the INVLPG
    // instruction. Otherwise, we must flush the entire TLB.
    if (use_pge)
        asm volatile ("invlpg (%0)" : : "r" (virtual_address));
    else
        kmem_page_flush_all();
}

void kmem_page_flush_all(void)
{
    uint32 cr;
    
    if (use_pge)
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
