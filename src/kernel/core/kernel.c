#include <typedef.h>
#include <multiboot.h>

#include <core/console.h>
#include <core/cpuid.h>
#include <core/gdt.h>
#include <core/idt.h>

void kernel_main(multiboot_info* mb_info);

void kernel_main(multiboot_info* mb_info)
{
    // Initialize basic console I/O
    console_init();
    
    // Run CPUID checks
    cpuid_init();
    
    // Initialize the GDT and IDT
    gdt_init();
    idt_init();
}
