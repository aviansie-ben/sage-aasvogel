#include <typedef.hpp>
#include <multiboot.hpp>
#include <tty.hpp>

#include <core/console.hpp>
#include <core/cpuid.hpp>
#include <core/gdt.hpp>
#include <core/idt.hpp>

using tty::boot_console;

extern "C" void kernel_main(multiboot_info* mb_info)
{
    console::init();
    
    boot_console.hideCursor();
    boot_console.activate();
    
    cpuid::init();
    
    gdt::init();
    idt::init();
    
    hang();
}
