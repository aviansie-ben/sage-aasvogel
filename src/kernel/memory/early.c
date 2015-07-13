#include <memory/early.h>
#include <assert.h>

#include <memory/phys.h>
#include <memory/page.h>

#include <core/elf.h>

static bool init_done = false;
static bool final_done = false;

addr_v kmem_early_next_alloc;
addr_v kmem_early_min_alloc;

extern const void _ld_kernel_end;

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
    
    r = kmem_early_next_alloc;
    if ((r & align_mask) != 0x0)
    {
        r |= align_mask;
        r += 1;
    }
    
    assert(r >= kmem_early_next_alloc && (r & align_mask) == 0x0);
    kmem_early_next_alloc = r + size;
    
    if (physical_address != NULL)
        *physical_address = r;
    
    return (void*)(r + 0xC0000000);
}

static inline void ensure_min_alloc_gte(uint32 addr)
{
    if (kmem_early_min_alloc < addr)
        kmem_early_min_alloc = addr;
}

void kmem_early_init(multiboot_info* multiboot)
{
    multiboot_module_entry* mod_entry;
    multiboot_module_entry* mod_end;
    
    elf32_shdr* shdr_entry;
    elf32_shdr* shdr_end;
    
    assert(!init_done && !final_done);
    
    kmem_early_min_alloc = (uint32)&_ld_kernel_end;
    
    if (multiboot->flags & MB_FLAG_ELF_SHDR)
    {
        ensure_min_alloc_gte(multiboot->elf_shdr_table.addr + (multiboot->elf_shdr_table.num * sizeof(elf32_shdr)));
        
        shdr_entry = (elf32_shdr*) (multiboot->elf_shdr_table.addr + 0xC0000000);
        shdr_end = (elf32_shdr*) ((uint32)shdr_entry + (multiboot->elf_shdr_table.num * multiboot->elf_shdr_table.size));
        
        while (shdr_entry != shdr_end)
        {
            ensure_min_alloc_gte(((shdr_entry->addr >= 0xC0000000) ? (shdr_entry->addr - 0xC0000000) : shdr_entry->addr) + shdr_entry->size);
            shdr_entry = (elf32_shdr*) ((uint32) shdr_entry + multiboot->elf_shdr_table.size);
        }
    }
    
    if (multiboot->flags & MB_FLAG_MODULES)
    {
        mod_entry = (multiboot_module_entry*) (multiboot->mods_addr + 0xC0000000);
        mod_end = mod_entry + multiboot->mods_count;
        
        while (mod_entry != mod_end)
        {
            ensure_min_alloc_gte(mod_entry->mod_end);
            mod_entry++;
        }
    }
    
    if ((kmem_early_min_alloc & 0xfff) != 0)
    {
        kmem_early_min_alloc |= 0xfff;
        kmem_early_min_alloc += 1;
    }
    
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
