#include <typedef.h>
#include <multiboot.h>

#include <core/gdt.h>
#include <core/idt.h>

void kernel_main(multiboot_info* mb_info);

void kernel_main(multiboot_info* mb_info)
{
    // Initialize the GDT and IDT
    gdt_init();
    idt_init();
}
