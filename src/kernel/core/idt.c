#include <core/idt.h>
#include <hwio.h>
#include <assert.h>

#define MASTER_PIC_COMMAND 0x20
#define MASTER_PIC_DATA 0x21
#define SLAVE_PIC_COMMAND 0xA0
#define SLAVE_PIC_DATA 0xA1

// Bit 0x10 lets the PIC know that this is the first initialization control word
// (ICW) and that it should reset. Bit 0x01 lets the PIC know that we will be
// sending ICW4 later.
#define COMMAND_ICW1 0x11

// This lets the master PIC know that there is a slave PIC connected to IRQ2.
#define COMMAND_ICW3_MASTER 0x04

// This tells the slave PIC that it has slave ID 2 and should communicate on
// IRQ2.
#define COMMAND_ICW3_SLAVE 0x02

// This lets the PIC know that it should operate in 8086 mode, that automatic
// EOI should be disabled, that it should operate in non-buffered mode, and
// that it is not in special fully nested mode.
#define COMMAND_ICW4 0x01

#define COMMAND_EOI 0x20
#define COMMAND_READ_ISR 0x0A

typedef void (*asm_int_handler)(void);

// These point to arrays of pointers to assembly functions that provide
// boilerplate code for dealing with ISRs and IRQs. These are mapped into the
// IDT when it is initialized.
extern asm_int_handler _isr_handlers[IDT_NUM_ISRS];
extern asm_int_handler _irq_handlers[IDT_NUM_IRQS];
extern asm_int_handler _ext_handlers[IDT_NUM_EXT];

static idt_pointer idt_ptr;
static idt_entry idt_entries[IDT_NUM_ENTRIES];

static interrupt_handler default_isr_handler = do_crash_unhandled_isr;
static interrupt_handler isr_handlers[IDT_NUM_ISRS];
static interrupt_handler irq_handlers[IDT_NUM_IRQS];
static interrupt_handler ext_handlers[IDT_NUM_EXT];

void _idt_handle(regs32* r) __hidden;
bool _idt_is_ring0(regs32* r) __hidden;

static void remap_pic(uint8 offset_master, uint8 offset_slave)
{
    uint8 mask_master, mask_slave;
    
    // Save the mask registers (they are cleared during initialization, and must
    // be restored later)
    mask_master = inb(MASTER_PIC_DATA);
    mask_slave = inb(SLAVE_PIC_DATA);
    
    // Send ICW1 to reset the PICs
    outb(MASTER_PIC_COMMAND, COMMAND_ICW1);
    outb(SLAVE_PIC_COMMAND, COMMAND_ICW1);
    io_wait();
    
    // Send ICW2 to tell the PICs what IDT offsets they should use
    outb(MASTER_PIC_DATA, offset_master);
    outb(SLAVE_PIC_DATA, offset_slave);
    io_wait();
    
    // Send ICW3 to tell the master and slave PICs about each other
    outb(MASTER_PIC_DATA, COMMAND_ICW3_MASTER);
    outb(SLAVE_PIC_DATA, COMMAND_ICW3_SLAVE);
    io_wait();
    
    // Send ICW4 to tell the master and slave what mode they should operate in
    outb(MASTER_PIC_DATA, COMMAND_ICW4);
    outb(SLAVE_PIC_DATA, COMMAND_ICW4);
    io_wait();
    
    // Restore the mask registers that were saved above
    outb(MASTER_PIC_DATA, mask_master);
    outb(SLAVE_PIC_DATA, mask_slave);
}

static void mask_all_pic_irqs(void)
{
    // Set the IMR on the master and slave PICs to have all interrupts masked.
    // IRQ 2 is explicitly not masked, since it is only used for communicating
    // between the master and slave PICs.
    outb(MASTER_PIC_DATA, 0xFB);
    outb(SLAVE_PIC_DATA, 0xFF);
}

static void set_pic_irq_masked(uint8 irq, bool masked)
{
    uint16 port;
    uint8 bit, imr;
    
    // IRQs 0-7 are on the master PIC, while IRQs 8-15 are on the slave PIC
    port = (irq > 7) ? SLAVE_PIC_DATA : MASTER_PIC_DATA;
    bit = (uint8)(1 << (irq % 8));
    
    // Read the existing IMR value
    imr = inb(port);
    
    // Set or clear the mask bit for the requested IRQ
    if (masked) imr |= bit;
    else imr &= (uint8)(~bit);
    
    // Write the new IMR value back to the PIC
    outb(port, imr);
}

static bool get_pic_irq_masked(uint8 irq)
{
    uint16 port;
    uint8 bit, imr;
    
    // IRQs 0-7 are on the master PIC, while IRQs 8-15 are on the slave PIC
    port = (irq > 7) ? SLAVE_PIC_DATA : MASTER_PIC_DATA;
    bit = (uint8)(1 << (irq % 8));
    
    // Read the existing IMR value and find the requested bit
    imr = inb(port);
    return (imr & bit) == bit;
}

void idt_init(void)
{
    uint32 i;
    // TODO: Use APIC when available instead of the PIC
    
    // Set up the IDT pointer
    idt_ptr.base = (uint32)&idt_entries;
    idt_ptr.size = sizeof(idt_entries) - 1;
    
    // All interrupts here are initialized with a segment of 0x08 (the kernel
    // code segment) and with flags 0x8E. (interrupt gate, DPL 0 (i.e. cannot
    // be called using INT from userland))
    
    // Load the ISR handlers into the IDT
    for (i = 0; i < IDT_NUM_ISRS; i++) idt_set_entry(i, (uint32) _isr_handlers[i], 0x08, 0x8E);
    
    // Load the IRQ handlers into the IDT
    for (i = 0; i < IDT_NUM_IRQS; i++) idt_set_entry(IDT_IRQS_START + i, (uint32) _irq_handlers[i], 0x08, 0x8E);
    
    // Load the extended interrupt handlers into the IDT
    for (i = 0; i < IDT_NUM_EXT; i++) idt_set_entry(IDT_EXT_START + i, (uint32) _ext_handlers[i], 0x08, 0xEF);
    
    // Flush the IDT
    idt_flush(&idt_ptr);
    
    // Tell the PIC to mask IRQs. Any necessary IRQs will be manually unmasked
    // later.
    mask_all_pic_irqs();
    
    // Remap the PIC to have IRQs 0-7 at IDT entries 0x20-0x27 and IRQs 8-15 at
    // IDT entries 0x28-0x2F. If we don't remap these, then IRQs 0-7 will
    // conflict with ISRs 8-15, and we won't be able to differentiate them.
    remap_pic(IDT_IRQS_START, IDT_IRQS_START + 8);
}

void idt_set_entry(uint32 n, uint32 offset, uint16 selector, uint8 flags)
{
    idt_entry* e;
    assert(n < IDT_NUM_ENTRIES);
    
    e = &idt_entries[n];
    
    // Set the IDT entry's offset address
    e->offset_low = (uint16)(offset & 0xFFFF);
    e->offset_high = (uint16)((offset >> 16) & 0xFFFF);
    
    // Set the IDT entry's selector and flags
    e->selector = selector;
    e->flags = flags;
}

void idt_register_isr_handler(uint32 n, interrupt_handler handler)
{
    assert(n < IDT_NUM_ISRS);
    
    isr_handlers[n] = handler;
}

void idt_register_default_isr_handler(interrupt_handler handler)
{
    assert(handler != 0);
    
    default_isr_handler = handler;
}

void idt_register_irq_handler(uint32 n, interrupt_handler handler)
{
    assert(n < IDT_NUM_IRQS);
    assert(handler != 0 || get_pic_irq_masked((uint8)n));
    
    irq_handlers[n] = handler;
}

void idt_set_irq_enabled(uint32 n, bool enabled)
{
    assert(n < IDT_NUM_IRQS);
    assert(irq_handlers[n] != 0 || !enabled);
    
    set_pic_irq_masked((uint8)n, !enabled);
}

void idt_set_ext_handler_flags(uint32 n, uint8 flags)
{
    assert(n < IDT_NUM_EXT);
    idt_entries[IDT_EXT_START + n].flags = flags;
}

void idt_register_ext_handler(uint32 n, interrupt_handler handler)
{
    assert(n < IDT_NUM_EXT);
    
    ext_handlers[n] = handler;
}

static uint16 read_pic_isr(void)
{
    // Send the ISR read command to the master and slave PICs
    outb(MASTER_PIC_COMMAND, COMMAND_READ_ISR);
    outb(SLAVE_PIC_COMMAND, COMMAND_READ_ISR);
    
    // Read the results back
    return (uint16)((inb(SLAVE_PIC_COMMAND) << 8) | inb(MASTER_PIC_COMMAND));
}

static bool irq_begin(regs32* r)
{
    uint8 irq = (uint8)(r->int_no - IDT_IRQS_START);
    
    // IRQs 7 and 15 may be spurious. We should check for that case before
    // handling the IRQ.
    if (irq == 7 && (read_pic_isr() & (1 << 7)) == 0)
    {
        return false; // Ignore spurious IRQ 7
    }
    else if (irq == 15 && (read_pic_isr() & (1 << 15)) == 0)
    {
        // The slave PIC knows that this is a spurious IRQ, but the master PIC
        // doesn't. We need to send an EOI to the master PIC to alert it.
        outb(MASTER_PIC_COMMAND, COMMAND_EOI);
        
        return false; // Ignore spurious IRQ 15
    }
    else
    {
        return true;
    }
}

static void irq_end(regs32* r)
{
    uint8 irq = (uint8)(r->int_no - IDT_IRQS_START);
    
    // If the IRQ came from the slave PIC, we need to let it know that we're
    // done handling the interrupt.
    if (irq > 7)
        outb(SLAVE_PIC_COMMAND, COMMAND_EOI);
    
    // Always let the master PIC know we're done
    outb(MASTER_PIC_COMMAND, COMMAND_EOI);
}

bool _idt_is_ring0(regs32* r)
{
    return ((r->cs & 0x3) == 0x0);
}

void _idt_handle(regs32* r)
{
    interrupt_handler handler = 0;
    
    // Find the proper handler and perform IRQ pre-handling code if needed
    if (r->int_no < IDT_NUM_ISRS)
    {
        handler = isr_handlers[r->int_no];
        
        if (handler == 0)
            handler = default_isr_handler;
    }
    else if (r->int_no >= IDT_IRQS_START && r->int_no < IDT_IRQS_START + IDT_NUM_IRQS)
    {
        handler = irq_handlers[r->int_no - IDT_IRQS_START];
        
        if (!irq_begin(r))
            return;
    }
    else if (r->int_no >= IDT_EXT_START && r->int_no < IDT_EXT_START + IDT_NUM_EXT)
    {
        handler = ext_handlers[r->int_no - IDT_EXT_START];
    }
    
    // Call the relevant handler
    if (handler != 0) (*handler)(r);
    else if (r->int_no < IDT_NUM_ISRS) crash("Unhandled ISR!");
    
    // Perform IRQ post-handling code if needed
    if (r->int_no >= IDT_IRQS_START && r->int_no < IDT_IRQS_START + IDT_NUM_IRQS)
    {
        irq_end(r);
    }
}
