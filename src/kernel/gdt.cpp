#include <gdt.hpp>

namespace gdt
{
    gdt_ptr pointer;
    gdt_entry entries[NUM_GDT_ENTRIES];
    tss_entry tss_entries[NUM_TSS_ENTRIES];
    
    extern "C" void gdt_flush(gdt_ptr* pointer);
    extern "C" void tss_load(uint32 n);
    
    void init()
    {
        // Set up the GDT pointer structure
        pointer.base = (uint32) &entries - 0xC0000000;
        pointer.limit = sizeof(gdt_entry) * NUM_GDT_ENTRIES - 1;
        
        // NOTE: The segments MUST be in this order for SYSENTER/SYSEXIT to function
        //       properly!
        
        // 0x00 is the NULL descriptor
        set(0, 0x00000000, 0x00000000, 0x00, 0x0);
        
        // 0x08 is the kernel code segment
        set(1, 0x00000000, 0xFFFFFFFF, 0x9A, 0xC);
        
        // 0x10 is the kernel data segment
        set(2, 0x00000000, 0xFFFFFFFF, 0x92, 0xC);
        
        // 0x18 is the user code segment
        set(3, 0x00000000, 0xFFFFFFFF, 0xFA, 0xC);
        
        // 0x20 is the user data segment
        set(4, 0x00000000, 0xFFFFFFFF, 0xF2, 0xC);
        
        // Set up the TSS entries
        for (uint32 i = 0; i < NUM_TSS_ENTRIES; i++)
            set_tss(5 + i, &tss_entries[i]); // TODO: Initialize TSS values
        
        // Flush the GDT
        gdt_flush(&pointer);
    }
    
    void set(uint8 n, uint32 base, uint32 limit, uint8 access, uint8 flags)
    {
        if (n >= NUM_GDT_ENTRIES) return; // TODO: Crash?
        
        // Set the GDT entry's base address
        entries[n].base_low = base & 0xFFFF;
        entries[n].base_mid = (base >> 16) & 0xFF;
        entries[n].base_high = (base >> 24) & 0xFF;
        
        // Set the GDT entry's limit
        entries[n].limit_low = limit & 0xFFFF;
        entries[n].limit_high = (limit >> 16) & 0xF;
        
        // Set the access byte and flags
        entries[n].access = access;
        entries[n].flags = flags;
    }
    
    void set_tss(uint8 n, tss_entry* tss)
    {
        set(n, (uint32)&tss - 0xC0000000, sizeof(tss_entry), 0x98, 0x4);
    }
}
