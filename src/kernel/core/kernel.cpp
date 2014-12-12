#include <typedef.hpp>
#include <multiboot.hpp>
#include <tty.hpp>

#include <core/console.hpp>
#include <core/cpuid.hpp>
#include <core/gdt.hpp>
#include <core/idt.hpp>

#include <drivers/serial.hpp>

#include <memory/mem.hpp>

using tty::boot_console;

extern "C" void kernel_main(multiboot_info* mb_info)
{
    console::init();
    serial::init();
    
    boot_console.hideCursor();
    boot_console.activate();
    
    mem::init(mb_info);
    
    cpuid::init();
    
    gdt::init();
    idt::init();
    
    hang();
}
