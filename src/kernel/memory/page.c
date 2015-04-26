#include <core/cpuid.h>
#include <core/msr.h>
#include <memory/early.h>
#include <memory/phys.h>
#include <memory/page.h>
#include <assert.h>

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

static bool init_done = false;
static bool use_pae = false;
static bool use_pge = false;
static bool use_nx = false;

static void kmem_page_map_pae(page_context* c, uint32 virtual_address, uint64 flags, uint64 physical_address);
static void kmem_page_map_legacy(page_context* c, uint32 virtual_address, uint32 flags, uint32 physical_address);

static void kmem_page_init_map_many_pae(uint32 start, uint32 end, bool virt, uint64 flags)
{
    uint32 addr;
    
    if (virt)
    {
        start -= KERNEL_VIRTUAL_ADDRESS_BEGIN;
        end -= KERNEL_VIRTUAL_ADDRESS_BEGIN;
    }
    
    for (addr = start & ~0xfffu; addr < end; addr += 0x1000)
        kmem_page_map_pae(&kernel_page_context, addr + KERNEL_VIRTUAL_ADDRESS_BEGIN, flags, addr);
}

static void kmem_page_init_pae(const boot_param* param)
{
    page_dir_ptr_tab* pdpt;
    page_dir_pae* pd;
    uint32 phys;
    uint32 i;
    
    multiboot_module_entry* mod_entry;
    multiboot_module_entry* mod_end;
    
    // If the CPU supports the NX bit, we should enable it now
    use_nx = !cmdline_get_bool(param, "no_nx") && msr_is_supported() && cpuid_supports_feature_ext_edx(CPUID_FEATURE_EXT_EDX_NX);
    if (use_nx)
        msr_write(MSR_EFER, msr_read(MSR_EFER) | MSR_EFER_FLAG_NX);
    
    spinlock_init(&kernel_page_context.lock);
    
    pdpt = kernel_page_context.pae_pdpt = kmalloc_early(sizeof(page_dir_ptr_tab), __alignof__(page_dir_ptr_tab), &phys);
    kernel_page_context.physical_address = phys;
    pdpt->page_dir_phys[0] = pdpt->page_dir_phys[1] = pdpt->page_dir_phys[2] = 0;
    pdpt->page_dir_virt[0] = pdpt->page_dir_virt[1] = pdpt->page_dir_virt[2] = NULL;
    
    for (i = 0; i < (sizeof(pdpt->page_table_virt) / sizeof(*pdpt->page_table_virt)); i++)
        pdpt->page_table_virt[i] = NULL;
    
    pd = pdpt->page_dir_virt[3] = kmalloc_early(sizeof(page_dir_pae), __alignof__(page_dir_pae), &phys);
    pdpt->page_dir_phys[3] = phys | PDPT_ENTRY_PRESENT;
    
    for (i = 0; i < (sizeof(pd->page_table_phys) / sizeof(*pd->page_table_phys)); i++)
        pd->page_table_phys[i] = 0;
    
    // Map the lower 1MiB of memory directly
    kmem_page_init_map_many_pae(0x0, 0x100000, false, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    
    // Map the kernel's segments
    kmem_page_init_map_many_pae((uint32)&_ld_text_begin, (uint32)&_ld_text_end, true, PT_ENTRY_GLOBAL);
    kmem_page_init_map_many_pae((uint32)&_ld_rodata_begin, (uint32)&_ld_rodata_end, true, PT_ENTRY_NO_EXECUTE | PT_ENTRY_GLOBAL);
    kmem_page_init_map_many_pae((uint32)&_ld_data_begin, (uint32)&_ld_data_end, true, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    kmem_page_init_map_many_pae((uint32)&_ld_bss_begin, (uint32)&_ld_bss_end, true, PT_ENTRY_NO_EXECUTE | PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    
    // Map any modules loaded by the bootloader
    if (param->multiboot->flags & MB_FLAG_MODULES)
    {
        mod_entry = (multiboot_module_entry*) (param->multiboot->mods_addr + 0xC0000000);
        mod_end = mod_entry + param->multiboot->mods_count;
        
        while (mod_entry < mod_end)
        {
            kmem_page_init_map_many_pae(mod_entry->mod_start, mod_entry->mod_end, false, PT_ENTRY_NO_EXECUTE | PT_ENTRY_GLOBAL);
            mod_entry++;
        }
    }
}

static void kmem_page_init_map_many_legacy(uint32 start, uint32 end, bool virt, uint32 flags)
{
    uint32 addr;
    
    if (virt)
    {
        start -= KERNEL_VIRTUAL_ADDRESS_BEGIN;
        end -= KERNEL_VIRTUAL_ADDRESS_BEGIN;
    }
    
    for (addr = start & ~0xfffu; addr < end; addr += 0x1000)
        kmem_page_map_legacy(&kernel_page_context, addr + KERNEL_VIRTUAL_ADDRESS_BEGIN, flags, addr);
}

static void kmem_page_init_legacy(const boot_param* param)
{
    page_dir_legacy* pd;
    uint32 phys;
    uint32 i;
    
    multiboot_module_entry* mod_entry;
    multiboot_module_entry* mod_end;
    
    pd = kernel_page_context.legacy_dir = kmalloc_early(sizeof(page_dir_legacy), __alignof__(page_dir_legacy), &phys);
    kernel_page_context.physical_address = phys;
    
    for (i = 0; i < (sizeof(pd->page_table_virt) / sizeof(*pd->page_table_virt)); i++)
    {
        pd->page_table_virt[i] = NULL;
        pd->page_table_phys[i] = 0;
    }
    
    // Map the lower 1MiB of memory directly
    kmem_page_init_map_many_legacy(0x0, 0x100000, false, PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    
    // Map the kernel's segments
    kmem_page_init_map_many_legacy((uint32)&_ld_text_begin, (uint32)&_ld_text_end, true, PT_ENTRY_GLOBAL);
    kmem_page_init_map_many_legacy((uint32)&_ld_rodata_begin, (uint32)&_ld_rodata_end, true, PT_ENTRY_GLOBAL);
    kmem_page_init_map_many_legacy((uint32)&_ld_data_begin, (uint32)&_ld_data_end, true, PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    kmem_page_init_map_many_legacy((uint32)&_ld_bss_begin, (uint32)&_ld_bss_end, true, PT_ENTRY_WRITEABLE | PT_ENTRY_GLOBAL);
    
    // Map any modules loaded by the bootloader
    if (param->multiboot->flags & MB_FLAG_MODULES)
    {
        mod_entry = (multiboot_module_entry*) (param->multiboot->mods_addr + 0xC0000000);
        mod_end = mod_entry + param->multiboot->mods_count;
        
        while (mod_entry < mod_end)
        {
            kmem_page_init_map_many_legacy(mod_entry->mod_start, mod_entry->mod_end, false, PT_ENTRY_GLOBAL);
            mod_entry++;
        }
    }
}

void kmem_page_init(const boot_param* param)
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
        kmem_page_init_pae(param);
    else
        kmem_page_init_legacy(param);
    
    // Finalize the early memory manager. Any space it has allocated will be
    // reserved in physical memory and mapped into the new page context.
    kmem_early_finalize();
    
    // Now that we're done setting up the new page context, we can switch into
    // it.
    kmem_page_context_switch(&kernel_page_context);
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
    uint32 pto, po;
    page_table_legacy* t;
    
    pto = (virtual_address & 0xffc00000) >> 22;
    po = (virtual_address & 0x003ff000) >> 12;
    
    t = c->legacy_dir->page_table_virt[pto];
    
    if (t == NULL || (t->page_phys[po] & PT_ENTRY_PRESENT) == 0)
        return NULL;
    else
        return kmem_block_find(t->page_phys[po] & PAGE_PHYSICAL_ADDRESS_MASK_64);
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
    
    if (!use_nx)
        flags &= ~PD_ENTRY_NO_EXECUTE;
    
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

static void kmem_page_map_pae(page_context* c, uint32 virtual_address, uint64 flags, uint64 physical_address)
{
    page_table_pae* pt;
    uint32 pto, po;
    
    if (!use_nx)
        flags &= ~PT_ENTRY_NO_EXECUTE;
    
    if (!use_pge)
        flags &= (uint32)~PT_ENTRY_GLOBAL;
    
    assert(c != NULL);
    assert((physical_address & PAGE_PHYSICAL_ADDRESS_MASK_64) == physical_address);
    assert((flags & PAGE_PHYSICAL_ADDRESS_MASK_64) == 0);
    
    pto = (virtual_address & 0xffe00000) >> 21;
    po = (virtual_address & 0x001ff000) >> 12;
    
    pt = c->pae_pdpt->page_table_virt[pto];
    if (pt == NULL)
        pt = kmem_page_table_create_pae(c, pto, (virtual_address >= KERNEL_VIRTUAL_ADDRESS_BEGIN) ? PD_ENTRY_DEF_KERNEL : PD_ENTRY_DEF_USER);
    
    pt->page_phys[po] = physical_address | flags | PT_ENTRY_PRESENT;
}

static page_table_legacy* kmem_page_table_create_legacy(page_context* c, uint32 table_number, uint32 flags)
{
    page_table_legacy* pt;
    uint32 phys;
    uint32 i;
    
    // TODO: Use proper memory allocation once it's available
    c->legacy_dir->page_table_virt[table_number] = pt = kmalloc_early(sizeof(page_table_legacy), __alignof__(page_table_legacy), &phys);
    c->legacy_dir->page_table_phys[table_number] = phys | flags | PD_ENTRY_PRESENT;
    
    for (i = 0; i < sizeof(pt->page_phys) / sizeof(*pt->page_phys); i++)
        pt->page_phys[i] = 0;
    
    return pt;
}

static void kmem_page_map_legacy(page_context* c, uint32 virtual_address, uint32 flags, uint32 physical_address)
{
    page_table_legacy* pt;
    uint32 pto, po;
    
    assert(c != NULL);
    assert((physical_address & PAGE_PHYSICAL_ADDRESS_MASK_32) == physical_address);
    assert((flags & PAGE_PHYSICAL_ADDRESS_MASK_32) == 0);
    
    if (!use_pge)
        flags &= (uint32)~PT_ENTRY_GLOBAL;
    
    pto = (virtual_address & 0xffc00000) >> 22;
    po = (virtual_address & 0x003ff000) >> 12;
    
    pt = c->legacy_dir->page_table_virt[pto];
    if (pt == NULL)
        pt = kmem_page_table_create_legacy(c, pto, (virtual_address >= KERNEL_VIRTUAL_ADDRESS_BEGIN) ? PD_ENTRY_DEF_KERNEL : PD_ENTRY_DEF_USER);
    
    pt->page_phys[po] = physical_address | flags | PT_ENTRY_PRESENT;
}

void kmem_page_map(page_context* c, uint32 virtual_address, uint64 flags, bool flush, mem_block* block)
{
    assert(c != NULL);
    assert(block != NULL);
    
    spinlock_acquire(&c->lock);
    
    if (use_pae)
        kmem_page_map_pae(c, virtual_address, flags, kmem_block_address(block));
    else
        kmem_page_map_legacy(c, virtual_address, (uint32)flags, (uint32)kmem_block_address(block));
    
    if (flush)
        kmem_page_flush_one(virtual_address);
    
    spinlock_release(&c->lock);
}

static void kmem_page_unmap_pae(page_context* c, uint32 virtual_address)
{
    page_table_pae* pt;
    uint32 pto, po;
    
    assert(c != NULL);
    
    pto = (virtual_address & 0xffe00000) >> 21;
    po = (virtual_address & 0x001ff000) >> 12;
    
    pt = c->pae_pdpt->page_table_virt[pto];
    
    if (pt != NULL)
        pt->page_phys[po] = 0;
}

static void kmem_page_unmap_legacy(page_context* c, uint32 virtual_address)
{
    page_table_legacy* pt;
    uint32 pto, po;
    
    assert(c != NULL);
    
    pto = (virtual_address & 0xffc00000) >> 22;
    po = (virtual_address & 0x003ff000) >> 12;
    
    pt = c->legacy_dir->page_table_virt[pto];
    
    if (pt != NULL)
        pt->page_phys[po] = 0;
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

static void kmem_page_global_map_pae(uint32 virtual_address, uint64 flags, bool flush, mem_block* block)
{
    spinlock_acquire(&kernel_page_context.lock);
    
    kmem_page_map_pae(&kernel_page_context, virtual_address, flags, kmem_block_address(block));
    
    if (flush)
        kmem_page_flush_one(virtual_address);
    
    spinlock_release(&kernel_page_context.lock);
}

static void kmem_page_global_map_legacy(uint32 virtual_address, uint32 flags, bool flush, mem_block* block)
{
    crash("Global page mapping for legacy paging is not yet supported!");
}

void kmem_page_global_map(uint32 virtual_address, uint64 flags, bool flush, mem_block* block)
{
    assert(virtual_address >= KERNEL_VIRTUAL_ADDRESS_BEGIN);
    assert(block != NULL);
    
    if (use_pae)
        kmem_page_global_map_pae(virtual_address, flags, flush, block);
    else if (use_pge)
        kmem_page_global_map_legacy(virtual_address, (uint32) flags, flush, block);
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
