#include <core/idt.hpp>
#include <core/crash.hpp>
#include <hwio.hpp>

typedef void (*asm_int_handler)();

// This points to an array of pointers to assembly functions that are designed
// to handle ISRs. These are mapped into the IDT at entries 0-31.
extern asm_int_handler _isr_handlers[32];

// This points to an array of pointers to assembly functions that are designed
// to handle IRQs. These are mapped into the IDT at entries 32-47.
extern asm_int_handler _irq_handlers[16];

namespace idt
{
    const uint16 MASTER_PIC_COMMAND = 0x20;
    const uint16 MASTER_PIC_DATA = 0x21;
    const uint16 SLAVE_PIC_COMMAND = 0xA0;
    const uint16 SLAVE_PIC_DATA = 0xA1;
    
    // Bit 0x10 lets the PIC know that this is the first initialization control word
    // (ICW) and that it should reset. Bit 0x01 lets the PIC know that we will be
    // sending ICW4 later.
    const uint16 COMMAND_ICW1 = 0x11;
    
    // This lets the master PIC know that there is a slave PIC connected to IRQ2.
    const uint16 COMMAND_ICW3_MASTER = 0x04;
    
    // This tells the slave PIC that it has slave ID 2 and should communicate on
    // IRQ2.
    const uint16 COMMAND_ICW3_SLAVE = 0x02;
    
    // This lets the PIC know that it should operate in 8086 mode, that automatic
    // EOI should be disabled, that it should operate in non-buffered mode, and
    // that it is not in special fully nested mode.
    const uint16 COMMAND_ICW4 = 0x01;
    
    const uint16 COMMAND_EOI = 0x20;
    const uint16 COMMAND_READ_ISR = 0x0A;
    
    idt_ptr pointer;
    idt_entry entries[NUM_IDT_ENTRIES];
    
    interrupt_handler default_isr_handler;
    interrupt_handler isr_handlers[NUM_ISRS];
    interrupt_handler irq_handlers[NUM_IRQS];
    interrupt_handler ext_handlers[NUM_EXT];
    
    extern "C" void idt_flush(idt_ptr* pointer);
    
    static void remap_pic(uint8 offset_master, uint8 offset_slave)
    {
        // Save the mask registers (they are cleared by initialization, and must be
        // restored later)
        uint8 mask_master, mask_slave;
        mask_master = inb(MASTER_PIC_DATA);
        mask_slave = inb(SLAVE_PIC_DATA);
        
        // Send ICW1 to reset the PICs
        outb(MASTER_PIC_COMMAND, COMMAND_ICW1);
        outb(SLAVE_PIC_COMMAND, COMMAND_ICW1);
        io_wait();
        
        // Send ICW2 to tell the PICs what IDT offsets they should use
        outb(MASTER_PIC_COMMAND, offset_master);
        outb(SLAVE_PIC_COMMAND, offset_slave);
        io_wait();
        
        // Send ICW3 to tell the master and slave PICs about each other
        outb(MASTER_PIC_COMMAND, COMMAND_ICW3_MASTER);
        outb(SLAVE_PIC_COMMAND, COMMAND_ICW3_SLAVE);
        io_wait();
        
        // Send ICW4 to tell the master and slave what mode they should operate in
        outb(MASTER_PIC_COMMAND, COMMAND_ICW4);
        outb(SLAVE_PIC_COMMAND, COMMAND_ICW4);
        io_wait();
        
        // We now restore the mask registers that we saved above
        outb(MASTER_PIC_DATA, mask_master);
        outb(SLAVE_PIC_DATA, mask_slave);
    }
    
    static void mask_all_pic_irqs()
    {
        // Set the IMR on the master and slave PICs to have all interrupts masked
        // (all bits set)
        outb(MASTER_PIC_DATA, 0xFF);
        outb(SLAVE_PIC_DATA, 0xFF);
    }
    
    static void set_pic_irq_masked(uint8 irq, bool masked)
    {
        // IRQs 0-7 are on the master PIC and IRQs 8-15 are on the slave PIC
        uint16 port = (irq > 7) ? SLAVE_PIC_DATA : MASTER_PIC_DATA;
        uint8 bit = (irq > 7) ? (1 << (irq - 7)) : 1 << irq;
        
        // Read the existing interrupt mask register value
        uint8 imr = inb(port);
        
        // Set or unset the mask bit for the requested IRQ
        if (masked) imr |= bit;
        else imr &= ~bit;
        
        // Write the new IMR back
        outb(port, imr);
    }
    
    void init()
    {
        // TODO: Add code to use the APIC instead of the PIC when it is available
        
        // Set up the IDT pointer structure
        pointer.base = (uint32) &entries - 0xC0000000;
        pointer.size = sizeof(entries) - 1;
        
        // All interrupts here are initialized with a segment of 0x08 (the kernel
        // code segment) and with flags 0x8E. (interrupt gate, DPL 0 (i.e. cannot
        // be called using INT from userland))
        
        // Load the ISR handlers into the IDT.
        for (uint32 i = 0; i < NUM_ISRS; i++) set(i, (uint32) _isr_handlers[i], 0x08, 0x8E);
        
        // Load the IRQ handlers into the IDT
        for (uint32 i = 0; i < NUM_IRQS; i++) set(i + 32, (uint32) _irq_handlers[i], 0x08, 0x8E);
        
        // TODO: Load extended interrupt handlers into the IDT
        
        // Flush the IDT to the CPU
        idt_flush(&pointer);
        
        // Tell the PIC to mask all IRQs. We will explicitly enable IRQs that we're
        // using when we load drivers for them.
        mask_all_pic_irqs();
        
        // Remap the PIC to have IRQs 0-7 at IDT entries 0x20-0x27 and IRQs 8-15 at
        // IDT entries 0x28-0x2F. If we don't do this, then IRQs 0-7 conflict with
        // ISRs 8-15, and we won't be able to differentiate them!
        remap_pic(IRQS_START, IRQS_START + 0x08);
    }
    
    void set(uint16 n, uint32 offset, uint16 selector, uint8 flags)
    {
        if (n >= NUM_IDT_ENTRIES)
            crash("Attempt to set IDT entry beyond end of IDT!");
        
        // Set the offset address
        entries[n].offset_low = offset & 0xFFFF;
        entries[n].offset_high = offset >> 16;
        
        // Set the section selector and flags
        entries[n].selector = selector;
        entries[n].flags = flags;
    }
    
    void register_isr_handler(uint8 n, interrupt_handler handler)
    {
        if (n >= NUM_ISRS)
            crash("Attempt to set ISR handler for non-existent ISR!");
        
        isr_handlers[n] = handler;
    }
    
    void register_default_isr_handler(interrupt_handler handler)
    {
        default_isr_handler = handler;
    }
    
    void register_irq_handler(uint16 n, interrupt_handler handler)
    {
        if (n >= NUM_IRQS)
            crash("Attempt to set IRQ handler for non-existent IRQ!");
        
        irq_handlers[n] = handler;
    }
    
    void set_irq_enabled(uint8 n, bool enabled)
    {
        if (n >= NUM_IRQS)
            crash("Attempt to enable/disable non-existent IRQ!");
        
        set_pic_irq_masked(n, !enabled);
    }
    
    void register_extended_handler(uint8 n, interrupt_handler handler)
    {
        if (n >= NUM_EXT)
            crash("Attempt to set extended interrupt handler beyond maximum interrupt!");
        
        ext_handlers[n] = handler;
    }
    
    static uint16 read_pic_isr()
    {
        // Send the ISR read command to the master and slave PICs.
        outb(MASTER_PIC_COMMAND, COMMAND_READ_ISR);
        outb(SLAVE_PIC_COMMAND, COMMAND_READ_ISR);
        
        // Read the results back from the two PICs.
        return (inb(SLAVE_PIC_COMMAND) << 8) | inb(MASTER_PIC_COMMAND);
    }
    
    static bool irq_begin(regs32_t* reg)
    {
        // IRQs 7 and 15 may be spurious. Check for that before handling the IRQ.
        if (reg->int_no - IRQS_START == 7 && (read_pic_isr() & (1 << 7)) == 0)
        {
            return false;
        }
        else if (reg->int_no - IRQS_START == 15 && (read_pic_isr() & (1 << 15)) == 0)
        {
            // The slave PIC knows that this is a spurious IRQ, but the master PIC
            // does not. Thus, we must send an EOI to the master PIC.
            outb(MASTER_PIC_COMMAND, COMMAND_EOI);
            return false;
        }
        else
        {
            return true;
        }
    }
    
    static void irq_end(regs32_t* reg)
    {
        // If the IRQ came from the slave PIC, let it know we're done with the
        // interrupt.
        if (reg->int_no - IRQS_START > 7) outb(SLAVE_PIC_COMMAND, COMMAND_EOI);
        
        // Always let the master PIC know we're done with the interrupt.
        outb(MASTER_PIC_COMMAND, COMMAND_EOI);
    }
    
    extern "C" void _idt_handle(regs32_t* reg)
    {
        interrupt_handler handler = 0;
        
        // Find the proper interrupt handler and perform IRQ pre-handling code
        if (reg->int_no < NUM_ISRS)
        {
            handler = isr_handlers[reg->int_no];
            
            if (handler == 0) handler = default_isr_handler;
        }
        else if (reg->int_no >= IRQS_START && reg->int_no < IRQS_START + NUM_IRQS)
        {
            handler = irq_handlers[reg->int_no];
            
            if (!irq_begin(reg)) return;
        }
        else if (reg->int_no >= EXT_INT_START && reg->int_no < EXT_INT_START + NUM_EXT)
        {
            handler = ext_handlers[reg->int_no];
        }
        
        // Call the ISR handler or crash if an ISR was unhandled.
        if (handler != 0) (*handler)(reg);
        else if (reg->int_no < NUM_ISRS) crash("Unhandled ISR!");
        
        // If we were handling an IRQ, we need to perform IRQ post-handling
        if (reg->int_no >= IRQS_START && reg->int_no < IRQS_START + NUM_IRQS)
        {
            irq_end(reg);
        }
    }
    
    extern "C" bool _idt_is_ring0(regs32_t* reg)
    {
        // The lower three bits represent the ring level of the function we were in
        // before the interrupt.
        return ((reg->cs & 3) == 0);
    }
}
