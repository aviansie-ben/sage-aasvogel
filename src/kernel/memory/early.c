#include <memory/early.h>
#include <assert.h>

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

void* kmalloc_early(size_t size, size_t align_mask, uint32* physical_address)
{
    uint32 r;
    
    assert(init_done && !final_done);
    assert(verify_mask(align_mask));
    
    r = next_alloc;
    if ((r & align_mask) != 0x0)
    {
        r |= ~r & align_mask;
        r += 1;
    }
    
    assert(r >= next_alloc && (r & align_mask) == 0x0);
    next_alloc = r + size;
    
    if (physical_address != NULL)
        *physical_address = r;
    
    return (void*)(r + 0xC0000000);
}

void kmem_early_init(multiboot_info* multiboot)
{
    multiboot_module_entry* mod_entry;
    multiboot_module_entry* mod_end;
    
    assert(!init_done && !final_done);
    
    min_alloc = (uint32)&_ld_kernel_end;
    
    if (multiboot->flags & MB_FLAG_MODULES)
    {
        mod_entry = (multiboot_module_entry*) (multiboot->mods_addr + 0xC0000000);
        mod_end = mod_entry + multiboot->mods_count;
        
        while (mod_entry < mod_end)
        {
            if (mod_entry->mod_end > min_alloc)
                min_alloc = mod_entry->mod_end;
        }
    }
    
    next_alloc = min_alloc;
    init_done = true;
}
