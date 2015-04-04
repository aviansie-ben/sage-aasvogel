#include <memory/phys.h>
#include <memory/early.h>
#include <lock.h>
#include <core/crash.h>
#include <core/tty.h>

#include <assert.h>

static mem_region* low_region = NULL;
static mem_region* high_region = NULL;

static mem_region* create_region(uint64 physical_address, uint16 num_blocks, uint32 block_flags)
{
    mem_region* region = kmalloc_early(sizeof(mem_region), __alignof__(mem_region), NULL);
    mem_block* block;
    uint32 i;
    
    spinlock_init(&region->lock);
    
    region->physical_address = physical_address;
    region->next = NULL;
    region->num_blocks = num_blocks;
    
    region->block_info = kmalloc_early(sizeof(mem_block) * num_blocks, __alignof__(mem_block), NULL);
    region->first_free = 0;
    
    for (i = 0; i < num_blocks; i++)
    {
        block = &region->block_info[i];
        
        block->region = region;
        block->ref_count = 0;
        block->flags = MEM_BLOCK_FREE | block_flags;
        block->owner_pid = 0;
        block->next_free = (i == (uint16)(num_blocks - 1)) ? (uint16)0xffff : (uint16)(i + 1);
    }
    
    return region;
}

void kmem_phys_init(multiboot_info* info)
{
    mem_region* prev_region = NULL;
    mem_region* next_region = NULL;
    uint64 high_blocks_left;
    uint64 i = 0;
    
    assert(low_region == NULL && high_region == NULL);
    
    if ((info->flags & (MB_FLAG_MEMORY | MB_FLAG_MEM_MAP)) != (MB_FLAG_MEMORY | MB_FLAG_MEM_MAP))
        crash("Bootloader did not provide memory information!");
    
    low_region = create_region(0x00000000, (uint16)(info->mem_lower >> 2), MEM_BLOCK_KERNEL_ONLY | MEM_BLOCK_HW_RESERVED);
    
    high_blocks_left = info->mem_upper >> 2;
    tprintf(&tty_virtual_consoles[0].base, "Detected %dKiB of high memory\n", info->mem_upper);
    
    while (high_blocks_left > 0)
    {
        next_region = create_region(0x00100000 + (i << 27), (high_blocks_left > 0x8000) ? (uint16)0x8000 : (uint16)high_blocks_left, 0);
        
        if (prev_region != NULL)
            prev_region->next = next_region;
        else
            high_region = next_region;
        
        high_blocks_left -= next_region->num_blocks;
        prev_region = next_region;
        i++;
    }
    
    low_region->next = high_region;
}

uint64 kmem_block_address(mem_block* block)
{
    return block->region->physical_address + (uint64)((block - block->region->block_info) << 12);
}

mem_block* kmem_block_find(uint64 address)
{
    mem_region* r;
    
    assert(low_region != NULL);
    
    address &= ~0xfffu;
    
    for (r = low_region; r != NULL; r = r->next)
    {
        if (address < (r->physical_address + (uint64)(r->num_blocks << 12)) && address >= r->physical_address)
        {
            return &r->block_info[(address - r->physical_address) >> 12];
        }
    }
    
    return NULL;
}

mem_block* kmem_block_alloc(bool kernel)
{
    mem_region* r;
    
    mem_block* pb;
    mem_block* b;
    
    assert(low_region != NULL);
    
    for (r = low_region; r != NULL; r = r->next)
    {
        if (r->first_free != 0xffff)
        {
            spinlock_acquire(&r->lock);
            for (pb = NULL, b = &r->block_info[r->first_free]; b != NULL; pb = b, b = &r->block_info[b->next_free])
            {
                assert((b->flags & MEM_BLOCK_FREE) == MEM_BLOCK_FREE);
                
                if (!kernel && (b->flags & MEM_BLOCK_KERNEL_ONLY) == 0)
                    continue;
                
                b->flags &= (uint32)~MEM_BLOCK_FREE;
                
                if (pb == NULL)
                    r->first_free = b->next_free;
                else
                    pb->next_free = b->next_free;
                
                spinlock_release(&r->lock);
                return b;
            }
            spinlock_release(&r->lock);
        }
    }
    
    return NULL;
}

void kmem_block_free(mem_block* block)
{
    mem_region* r;
    mem_block* pb;
    assert((block->flags & MEM_BLOCK_FREE) != MEM_BLOCK_FREE);
    
    r = block->region;
    
    spinlock_acquire(&r->lock);
    block->flags |= MEM_BLOCK_FREE;
    
    if (r->first_free == 0xffff || block - r->block_info < r->first_free)
    {
        block->next_free = r->first_free;
        r->first_free = (uint16)(block - r->block_info);
    }
    else
    {
        for (pb = &r->block_info[r->first_free]; pb->next_free != 0xffff && pb->next_free < block - r->block_info; pb = &r->block_info[pb->next_free]) ;
        block->next_free = pb->next_free;
        pb->next_free = (uint16)(block - r->block_info);
    }
    
    spinlock_release(&r->lock);
}
