#ifndef __GDT_HPP__
#define __GDT_HPP__

#include <typedef.hpp>

namespace gdt
{
    const uint32 IOPB_SIZE = 32;
    const uint32 NUM_GDT_ENTRIES = 10;
    const uint32 NUM_TSS_ENTRIES = 1;
    
    struct gdt_entry
    {
        uint16 limit_low;
        uint16 base_low;
        uint8 base_mid;
        uint8 access;
        uint8 limit_high : 4;
        uint8 flags : 4;
        uint8 base_high;
    } __attribute__((packed));
    
    struct gdt_ptr
    {
        uint16 limit;
        uint32 base;
    } __attribute__((packed));
    
    struct tss_entry
    {
        uint32 link;
        uint32 esp0, ss0, esp1, ss1, esp2, ss2;
        uint32 cr3;
        uint32 eip;
        uint32 eflags;
        uint32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
        uint32 es, cs, ss, ds, fs, gs;
        uint32 iopb_offset;
        
        uint8 iopb[IOPB_SIZE];
    };
    
    void init();
    void set(uint8 n, uint32 base, uint32 limit, uint8 access, uint8 flags);
    void set_tss(uint8 n, tss_entry* tss);
}

#endif
