#include <memory/mem.hpp>
#include <core/crash.hpp>

namespace mem
{
    static uint8 init_stage = 0;
    
    static uint8 prepaging_static[KB_PREPAGING_STATIC * 1024] __attribute__((aligned(0x1000)));
    
    static uint8* current_prepaging_static = &prepaging_static[0];
    static uint8* end_prepaging_static = &prepaging_static[0] + sizeof(prepaging_static);
    
    void init(multiboot_info* mb_info)
    {
        // TODO: Implement this
    }
    
    void* kmalloc_prepaging_static(uint32 size, uint64* physical_addr, uint32 align_by)
    {
        uint8* ptr = current_prepaging_static;
        
        // If needed, align the address we will be allocating at
        if (align_by > 0 && ((uint32)ptr) % align_by > 0)
            ptr += align_by - (((uint32)ptr) % align_by);
        
        // If we run out of prepaging static before the memory manager is done
        // initializing, we cannot initialize the memory manager! Just crash...
        if (ptr + size > end_prepaging_static)
            crash("Ran out of prepaging static memory!");
        
        // If the physical address was requested, find it
        if (physical_addr != 0)
            *physical_addr = ((uint32)ptr) - 0xC0000000;
        
        // Move current_prepaging_static to point after the newly allocated block
        current_prepaging_static = ptr + size;
        
        return ptr;
    }
    
    void* kmalloc(uint32 size, uint64* physical_addr, uint32 align_by)
    {
        if (init_stage == 0) return kmalloc_prepaging_static(size, physical_addr, align_by);
        else return 0;
    }
    
    void kfree(void* ptr)
    {
        // Memory that was dynamically allocated in prepaging_static cannot be freed
        if (ptr >= &prepaging_static[0] && ptr < end_prepaging_static) return;
        
        // TODO: Free memory
    }
}
