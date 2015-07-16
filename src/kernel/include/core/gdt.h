#ifndef CORE_GDT_H
#define CORE_GDT_H

#include <typedef.h>

#define GDT_IOPB_SIZE 32
#define GDT_NUM_ENTRIES 10
#define GDT_NUM_TSS_ENTRIES 1

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20

typedef struct
{
    uint16 limit_low;
    uint16 base_low;
    uint8 base_mid;
    uint8 access;
    uint8 limit_high : 4;
    uint8 flags : 4;
    uint8 base_high;
} __attribute__((packed)) gdt_entry;

typedef struct
{
    uint16 limit;
    uint32 base;
} __attribute__((packed)) gdt_pointer;

typedef struct
{
    uint32 link;
    
    uint32 esp0, ss0, esp1, ss1, esp2, ss2;
    uint32 cr3, eip, eflags;
    uint32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32 es, cs, ss, ds, fs, gs;
    
    uint32 iopb_offset;
    uint8 iopb[GDT_IOPB_SIZE];
} __attribute__((packed)) tss_entry;

extern gdt_entry gdt_entries[GDT_NUM_ENTRIES];
extern tss_entry tss_entries[GDT_NUM_TSS_ENTRIES];

extern void gdt_init(void) __hidden;
extern void gdt_set(uint32 n, uint32 base, uint32 limit, uint8 access, uint8 flags);
extern void gdt_set_tss(uint32 n, tss_entry* tss);

extern void gdt_flush(gdt_pointer* ptr);
extern void tss_load(uint32 n);

#endif
