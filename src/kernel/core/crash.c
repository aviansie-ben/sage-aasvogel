#include <core/crash.h>
#include <core/tty.h>
#include <lock.h>

#include <string.h>
#include <core/unwind.h>

#include <core/ksym.h>

static void lookup_name(unsigned int address, char* out)
{
    char tmp[9];
    
    const kernel_symbol* symbol;
    uint32 symbol_offset;
    
    if ((symbol = ksym_address_lookup(address, &symbol_offset)) != NULL)
    {
        itoa((int) symbol_offset, tmp, 16);
        
        strcpy(out, symbol->name);
        strcat(out, "+0x");
        strcat(out, tmp);
    }
    else
    {
        itoa((int) address, tmp, 16);
        
        strcpy(out, "0x");
        strcat(out, tmp);
    }
}

static void crash_print_stackframe(unsigned int eip, void* esp)
{
    char name[50];
    
    lookup_name(eip, name);
    tprintf(&tty_virtual_consoles[0].base, "  %s\n", name);
}

void do_crash(const char* msg, const char* file, const char* func, uint32 line)
{
    asm volatile ("cli");
    
    tty_switch_vc(&tty_virtual_consoles[0]);
    
    spinlock_acquire(&tty_virtual_consoles[0].base.lock);
    tty_virtual_consoles[0].base.fore_color = CONSOLE_COLOR_WHITE;
    tty_virtual_consoles[0].base.back_color = CONSOLE_COLOR_RED;
    tty_virtual_consoles[0].base.clear(&tty_virtual_consoles[0].base);
    tty_virtual_consoles[0].base.cursor_hidden = true;
    spinlock_release(&tty_virtual_consoles[0].base.lock);
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  %s\n  Location: %s line %d\n  Function: %s\n\nStack Trace:\n", msg, file, line, func);
    unchecked_unwind_here(1, CRASH_STACKTRACE_MAX_DEPTH, crash_print_stackframe);
    
    hang();
}

void do_crash_unhandled_isr(regs32_t* r)
{
    asm volatile ("cli");
    
    tty_switch_vc(&tty_virtual_consoles[0]);
    
    spinlock_acquire(&tty_virtual_consoles[0].base.lock);
    tty_virtual_consoles[0].base.fore_color = CONSOLE_COLOR_WHITE;
    tty_virtual_consoles[0].base.back_color = CONSOLE_COLOR_RED;
    tty_virtual_consoles[0].base.clear(&tty_virtual_consoles[0].base);
    tty_virtual_consoles[0].base.cursor_hidden = true;
    spinlock_release(&tty_virtual_consoles[0].base.lock);
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  Unexpected ISR: 0x%x\n\nStack Trace:\n", r->int_no);
    unchecked_unwind(r->eip, (void*) r->ebp, CRASH_STACKTRACE_MAX_DEPTH, crash_print_stackframe);
    
    hang();
}
