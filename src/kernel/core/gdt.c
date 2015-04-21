#include <core/gdt.h>
#include <assert.h>

static gdt_pointer gdt_ptr;
gdt_entry gdt_entries[GDT_NUM_ENTRIES];
tss_entry tss_entries[GDT_NUM_TSS_ENTRIES];

void gdt_init(void)
{
    uint32 i;
    
    // Set up the GDT pointer
    gdt_ptr.base = (uint32)&gdt_entries;
    gdt_ptr.limit = sizeof(gdt_entry) * GDT_NUM_ENTRIES - 1;
    
    // 0x00 is the NULL descriptor
    gdt_set(0, 0x00000000, 0x00000000, 0x00, 0x0);
    
    // 0x08 is the kernel code segment
    gdt_set(1, 0x00000000, 0xFFFFFFFF, 0x9A, 0xC);
    
    // 0x10 is the kernel data segment
    gdt_set(2, 0x00000000, 0xFFFFFFFF, 0x92, 0xC);
    
    // 0x18 is the user code segment
    gdt_set(3, 0x00000000, 0xFFFFFFFF, 0xFA, 0xC);
    
    // 0x20 is the user data segment
    gdt_set(4, 0x00000000, 0xFFFFFFFF, 0xF2, 0xC);
    
    // TODO: Initialize TSS entry values
    
    for (i = 0; i < GDT_NUM_TSS_ENTRIES; i++)
        gdt_set_tss(GDT_NUM_ENTRIES - i - 1, &tss_entries[i]);
    
    // Flush the GDT
    gdt_flush(&gdt_ptr);
}

void gdt_set(uint32 n, uint32 base, uint32 limit, uint8 access, uint8 flags)
{
    gdt_entry* e;
    assert(n < GDT_NUM_ENTRIES);
    
    e = &gdt_entries[n];
    
    // Set the GDT entry's base address
    e->base_low = (uint16)(base & 0xFFFF);
    e->base_mid = (uint8)((base >> 16) & 0xFF);
    e->base_high = (uint8)((base >> 24) & 0xFF);
    
    // Set the GDT entry's limit
    e->limit_low = (uint16)(limit & 0xFFFF);
    e->limit_high = (uint8)((limit >> 16) & 0xF);
    
    // Set the access byte and flags
    e->access = access;
    e->flags = (uint8)(flags & 0xF);
}

void gdt_set_tss(uint32 n, tss_entry* tss)
{
    gdt_set(n, (uint32)&tss - 0xC0000000, sizeof(tss_entry), 0x98, 0x4);
}
