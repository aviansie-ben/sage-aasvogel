#include <typedef.h>
#include <multiboot.h>

#include <core/console.h>
#include <core/cpuid.h>
#include <core/gdt.h>
#include <core/idt.h>

#include <core/tty.h>
#include <assert.h>
#include <core/version.h>

#include <core/unwind.h>

void kernel_main(multiboot_info* mb_info);

void kernel_main(multiboot_info* mb_info)
{
    // Initialize basic console I/O
    console_init();
    tty_init();
    
    tprintf(&tty_virtual_consoles[0].base, "Booting " OS_NAME " v" STRINGIFY(MAJOR_VERSION) "." STRINGIFY(MINOR_VERSION) "...\n");
    
    // Run CPUID checks
    cpuid_init();
    
    tprintf(&tty_virtual_consoles[0].base, "Detected CPU Vendor: %s (%s)\n", cpuid_detected_vendor->vendor_name, cpuid_detected_vendor->vendor_id.str);
    
    // Initialize the GDT and IDT
    gdt_init();
    idt_init();
    
    hang();
}
