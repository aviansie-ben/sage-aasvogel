#include <memory/early.h>
#include <assert.h>

#include <memory/phys.h>
#include <memory/page.h>

#include <core/elf.h>

static bool init_done = false;
static bool final_done = false;

static uint32 next_alloc;
static uint32 min_alloc;

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
    
    r = next_alloc;
    if ((r & align_mask) != 0x0)
    {
        r |= align_mask;
        r += 1;
    }
    
    assert(r >= next_alloc && (r & align_mask) == 0x0);
    next_alloc = r + size;
    
    if (physical_address != NULL)
        *physical_address = r;
    
    return (void*)(r + 0xC0000000);
}

static inline void ensure_min_alloc_gte(uint32 addr)
{
    if (min_alloc < addr)
        min_alloc = addr;
}

void kmem_early_init(multiboot_info* multiboot)
{
    multiboot_module_entry* mod_entry;
    multiboot_module_entry* mod_end;
    
    elf32_shdr* shdr_entry;
    elf32_shdr* shdr_end;
    
    assert(!init_done && !final_done);
    
    min_alloc = (uint32)&_ld_kernel_end;
    
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
    
    if ((min_alloc & 0xfff) != 0)
    {
        min_alloc |= 0xfff;
        min_alloc += 1;
    }
    
    next_alloc = min_alloc;
    init_done = true;
}

void kmem_early_finalize(addr_v* alloc_begin, addr_v* alloc_end)
{
    assert(init_done);
    
    // Once we've finalized, the early memory manager can no longer allocate any
    // new memory.
    final_done = true;
    
    *alloc_begin = (addr_v)(min_alloc + 0xC0000000);
    *alloc_end = (addr_v)(next_alloc + 0xC0000000);
}
