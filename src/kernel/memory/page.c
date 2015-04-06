#include <core/cpuid.h>
#include <memory/early.h>
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
