#ifndef __IDT_HPP__
#define __IDT_HPP__

#include <typedef.hpp>

namespace idt
{
    const uint32 NUM_IDT_ENTRIES = 256;
    
    const uint32 NUM_ISRS = 32;
    
    const uint32 IRQS_START = 0x20;
    const uint32 NUM_IRQS = 16;
    
    const uint32 EXT_INT_START = 0x80;
    const uint32 NUM_EXT = 0; // TODO: Add extended interrupts
    
    typedef void (*interrupt_handler)(regs32_t* reg);
    
    struct idt_entry
    {
        uint16 offset_low;
        uint16 selector;
        uint8 res;
        uint8 flags;
        uint16 offset_high;
    } __attribute__((packed));
    
    struct idt_ptr
    {
        uint16 size;
        uint32 base;
    } __attribute__((packed));
    
    extern void init();
    extern void set(uint16 n, uint32 offset, uint16 selector, uint8 flags);
    
    extern void register_isr_handler(uint8 n, interrupt_handler handler);
    extern void register_default_isr_handler(interrupt_handler handler);
    
    extern void register_irq_handler(uint16 n, interrupt_handler handler);
    extern void set_irq_enabled(uint8 n, bool enabled);
    
    extern void register_extended_handler(uint8 n, interrupt_handler handler);
}

#endif
