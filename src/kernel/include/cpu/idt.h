#ifndef CORE_IDT_H
#define CORE_IDT_H

#define IDT_NUM_ENTRIES 256

#define IDT_NUM_ISRS 32

#define IDT_IRQS_START 0x20
#define IDT_NUM_IRQS 16

#define IDT_EXT_START 0x80
#define IDT_NUM_EXT 64

#ifndef __ASSEMBLY__
#include <typedef.h>

typedef void (*interrupt_handler)(regs32_t* regs);

typedef struct
{
    uint16 offset_low;
    uint16 selector;
    uint8 res;
    uint8 flags;
    uint16 offset_high;
} __attribute__((packed)) idt_entry;

typedef struct
{
    uint16 size;
    uint32 base;
} __attribute__((packed)) idt_pointer;

extern void idt_init(void) __hidden;
extern void idt_set_entry(uint32 n, uint32 offset, uint16 selector, uint8 flags);

extern void idt_register_isr_handler(uint32 n, interrupt_handler handler);
extern void idt_register_default_isr_handler(interrupt_handler handler);

extern void idt_register_irq_handler(uint32 n, interrupt_handler handler);
extern void idt_set_irq_enabled(uint32 n, bool enabled);

void idt_set_ext_handler_flags(uint32 n, uint8 flags);
extern void idt_register_ext_handler(uint32 n, interrupt_handler handler);

extern void idt_flush(idt_pointer* ptr);
#endif

#endif
