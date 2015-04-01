#include <memory/phys.h>
#include <memory/early.h>
#include <lock.h>
#include <core/crash.h>
#include <core/tty.h>

static mem_region* low_region;
static mem_region* high_region;

static mem_region* create_region(uint64 physical_address, uint16 num_blocks, uint32 block_flags)
{
    mem_region* region = kmalloc_early(sizeof(mem_region), 0x3, NULL);
    mem_block* block;
    uint32 i;
    
    spinlock_init(&region->lock);
    
    region->physical_address = physical_address;
    region->next = NULL;
    region->num_blocks = region->num_free = num_blocks;
    
    region->block_info = region->first_free = kmalloc_early(sizeof(mem_block) * num_blocks, 0x3, NULL);
    
    for (i = 0; i < num_blocks; i++)
    {
        block = &region->block_info[i];
        
        block->region = region;
        block->region_offset = (uint16)i;
        block->ref_count = 0;
        block->flags = MEM_BLOCK_FREE | block_flags;
        block->owner_pid = 0;
        block->next_free = ((uint16)i == num_blocks - 1) ? NULL : (block + 1);
    }
    
    return region;
}

void kmem_phys_init(multiboot_info* info)
{
    mem_region* prev_region = NULL;
    mem_region* next_region = NULL;
    uint64 high_blocks_left;
    uint64 i = 0;
    
    if ((info->flags & (MB_FLAG_MEMORY | MB_FLAG_MEM_MAP)) != (MB_FLAG_MEMORY | MB_FLAG_MEM_MAP))
        crash("Bootloader did not provide memory information!");
    
    low_region = create_region(0x00000000, (uint16)(info->mem_lower >> 2), MEM_BLOCK_KERNEL_ONLY);
    
    high_blocks_left = info->mem_upper >> 2;
    // TODO Add 64-bit integer support to tprintf
    tprintf(&tty_virtual_consoles[0].base, "Detected %dKiB of high memory\n", (uint32) info->mem_upper);
    
    while (high_blocks_left > 0)
    {
        next_region = create_region(0x00100000 + (i * 0x08000000), (high_blocks_left > 0x8000) ? (uint16)0x8000 : (uint16)high_blocks_left, 0);
        
        if (prev_region != NULL)
            prev_region->next = next_region;
        else
            high_region = next_region;
        
        high_blocks_left -= next_region->num_blocks;
        prev_region = next_region;
        i++;
    }
}
