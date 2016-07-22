#include <core/crash.h>
#include <core/tty.h>
#include <lock/spinlock.h>

#include <string.h>
#include <core/unwind.h>

#include <core/ksym.h>
#include <core/klog.h>
#include <core/serial.h>

static bool lookup_name(unsigned int address, char* out, uint32 flags)
{
    char tmp[9];
    
    const kernel_symbol* symbol;
    uint32 symbol_offset;
    
    if ((symbol = ksym_address_lookup(address, &symbol_offset, flags)) != NULL)
    {
        itoa((int) symbol_offset, tmp, 16);
        
        strcpy(out, symbol->name);
        strcat(out, "+0x");
        strcat(out, tmp);
        
        return true;
    }
    else
    {
        itoa((int) address, tmp, 16);
        
        strcpy(out, "0x");
        strcat(out, tmp);
        
        return false;
    }
}

static void crash_print_stackframe(unsigned int eip, void* esp)
{
    char name[50];
    
    lookup_name(eip, name, KSYM_ALOOKUP_RET);
    tprintf(&tty_virtual_consoles[0].base, "  %s\n", name);
    tprintf(&tty_serial_consoles[0].base, "  %s\n", name);
}

void do_crash(const char* msg, const char* file, const char* func, uint32 line)
{
    asm volatile ("cli");
    
    serial_stop_interrupts();
    klog_flush();
    tty_switch_vc(&tty_virtual_consoles[0]);
    
    tty_virtual_consoles[0].fore_color = CONSOLE_COLOR_WHITE;
    tty_virtual_consoles[0].back_color = CONSOLE_COLOR_RED;
    tty_virtual_consoles[0].base.clear(&tty_virtual_consoles[0].base);
    tty_virtual_consoles[0].base.cursor_hidden = true;
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  %s\n  Location: %s line %d (%s)\n\nStack Trace:\n", msg, file, line, func);
    tprintf(&tty_serial_consoles[0].base, "Kernel has crashed: %s\nStack Trace:\n", msg);
    unchecked_unwind_here(1, CRASH_STACKTRACE_MAX_DEPTH, crash_print_stackframe);
    
    hang();
}

void do_crash_interrupt(const char* msg, const char* file, const char* func, uint32 line, regs32_t* r)
{
    asm volatile ("cli");
    
    serial_stop_interrupts();
    klog_flush();
    tty_switch_vc(&tty_virtual_consoles[0]);
    
    tty_virtual_consoles[0].fore_color = CONSOLE_COLOR_WHITE;
    tty_virtual_consoles[0].back_color = CONSOLE_COLOR_RED;
    tty_virtual_consoles[0].base.clear(&tty_virtual_consoles[0].base);
    tty_virtual_consoles[0].base.cursor_hidden = true;
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  %s\n  Location: %s line %d\n  Function: %s\n\nStack Trace:\n", msg, file, line, func);
    tprintf(&tty_serial_consoles[0].base, "Kernel has crashed!\nStack Trace:\n");
    unchecked_unwind(r->eip, (void*) r->ebp, CRASH_STACKTRACE_MAX_DEPTH, crash_print_stackframe);
    
    hang();
}

void do_crash_pagefault(regs32_t* r, uint32 fault_address)
{
    char address_name[50];
    bool address_named;
    
    asm volatile ("cli");
    
    serial_stop_interrupts();
    klog_flush();
    tty_switch_vc(&tty_virtual_consoles[0]);
    
    tty_virtual_consoles[0].fore_color = CONSOLE_COLOR_WHITE;
    tty_virtual_consoles[0].back_color = CONSOLE_COLOR_RED;
    tty_virtual_consoles[0].base.clear(&tty_virtual_consoles[0].base);
    tty_virtual_consoles[0].base.cursor_hidden = true;
    
    address_named = lookup_name(fault_address, address_name, 0);
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  ");
    tprintf(&tty_serial_consoles[0].base, "Kernel has crashed: ");
    
    if ((r->err_code & 0x8) != 0)
    {
        tprintf(&tty_virtual_consoles[0].base, "Reserved bits set in page at ", fault_address);
        tprintf(&tty_serial_consoles[0].base, "reserved bits set in page");
    }
    else if ((r->err_code & 0x1) == 0)
    {
        tprintf(&tty_virtual_consoles[0].base, "Attempt to %s non-present memory at ", ((r->err_code & 0x10) != 0) ? "execute" : (((r->err_code & 0x2) != 0) ? "write" : "read"));
        tprintf(&tty_serial_consoles[0].base, "attempt to %s non-present memory", ((r->err_code & 0x10) != 0) ? "execute" : (((r->err_code & 0x2) != 0) ? "write" : "read"));
    }
    else if ((r->err_code & 0x10) != 0)
    {
        tprintf(&tty_virtual_consoles[0].base, "Attempt to execute non-executable memory at ");
        tprintf(&tty_serial_consoles[0].base, "executing non-executable memory");
    }
    else if ((r->err_code & 0x2) != 0)
    {
        tprintf(&tty_virtual_consoles[0].base, "Attempt to write to read-only memory at ");
        tprintf(&tty_serial_consoles[0].base, "writing read-only memory");
    }
    else
    {
        tprintf(&tty_virtual_consoles[0].base, "Attempt to read protected memory at ");
        tprintf(&tty_serial_consoles[0].base, "reading protected memory");
    }
    
    if (address_named)
    {
        tprintf(&tty_virtual_consoles[0].base, "%s [0x%08x]", address_name, fault_address);
        tprintf(&tty_serial_consoles[0].base, "\nFaulting address: %s [0x%08x]", address_name, fault_address);
    }
    else
    {
        tprintf(&tty_virtual_consoles[0].base, "0x%08x", fault_address);
        tprintf(&tty_serial_consoles[0].base, "\nFaulting address: 0x%08x", fault_address);
    }
    
    tprintf(&tty_virtual_consoles[0].base, "\n\nStack Trace:\n");
    tprintf(&tty_serial_consoles[0].base, "\nStack Trace:\n");
    unchecked_unwind(r->eip, (void*) r->ebp, CRASH_STACKTRACE_MAX_DEPTH, crash_print_stackframe);
    
    hang();
}

void do_crash_unhandled_isr(regs32_t* r)
{
    asm volatile ("cli");
    
    serial_stop_interrupts();
    klog_flush();
    tty_switch_vc(&tty_virtual_consoles[0]);
    
    tty_virtual_consoles[0].fore_color = CONSOLE_COLOR_WHITE;
    tty_virtual_consoles[0].back_color = CONSOLE_COLOR_RED;
    tty_virtual_consoles[0].base.clear(&tty_virtual_consoles[0].base);
    tty_virtual_consoles[0].base.cursor_hidden = true;
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  Unexpected ISR: 0x%x\n\nStack Trace:\n", r->int_no);
    tprintf(&tty_serial_consoles[0].base, "Kernel has crashed: unhandled isr 0x%x\nStack Trace:\n", r->int_no);
    unchecked_unwind(r->eip, (void*) r->ebp, CRASH_STACKTRACE_MAX_DEPTH, crash_print_stackframe);
    
    hang();
}
