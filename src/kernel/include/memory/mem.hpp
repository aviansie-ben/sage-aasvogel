#ifndef __MEM_HPP__
#define __MEM_HPP__

#include <typedef.hpp>
#include <multiboot.hpp>

namespace mem
{
    const uint32 KB_PREPAGING_STATIC = 64;
    
    extern void init(multiboot_info* mb_info);
    
    extern void* kmalloc(uint32 size, uint64* physical_addr, uint32 align_by);
    extern void kfree(void* ptr);
}

#endif
