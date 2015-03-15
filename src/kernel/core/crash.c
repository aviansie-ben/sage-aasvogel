#include <core/crash.h>
#include <core/tty.h>
#include <lock.h>

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
    
    tprintf(&tty_virtual_consoles[0].base, "Sage Aasvogel has crashed!\n  %s\n  Location: %s line %d\n  Function: %s\n", msg, file, line, func);
    hang();
}
