#include <typedef.h>
#include <multiboot.h>

#include <core/bootparam.h>

#include <core/console.h>
#include <core/cpuid.h>
#include <core/gdt.h>
#include <core/idt.h>
#include <core/ksym.h>

#include <core/tty.h>
#include <core/klog.h>
#include <assert.h>
#include <core/version.h>

#include <memory/early.h>
#include <memory/phys.h>
#include <memory/page.h>
#include <memory/virt.h>

#include <core/sched.h>

#include <fs/vfs.h>

#include <acpica/acpi.h>

static boot_param gparam;

void kernel_main(multiboot_info* multiboot);

static void kernel_main2(const boot_param* param)
{
    // Run CPUID checks
    cpuid_init();
    klog(KLOG_LEVEL_INFO, "Detected CPU Vendor: %s (%s)\n", cpuid_detected_vendor->vendor_name, cpuid_detected_vendor->vendor_id.str);
    
    // Initialize the GDT and IDT
    gdt_init();
    idt_init();
    
    // Initialize the memory manager
    kmem_page_init(param);
    kmem_phys_init(param);
    kmem_virt_init(param);
    kmem_pool_generic_init();
    
    // Initialize the CPU scheduler
    sched_init(param);
    klog_start_background_thread();
    
    // Now that the scheduler is ready, we can enable interrupts for TTYs
    tty_init_interrupts();
    
    vfs_init(param);
    
    ACPI_STATUS status;
    status = AcpiInitializeSubsystem();
    status = AcpiInitializeTables(NULL, 16, FALSE);
    status = AcpiLoadTables();
    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    
    while (true)
    {
        sched_thread_current()->status = STS_DEAD;
        sched_yield();
    }
}

void kernel_main(multiboot_info* multiboot)
{
    // Initialize basic console I/O
    console_init();
    tty_init();
    
    tprintf(&tty_virtual_consoles[0].base, "\33[37mBooting " OS_NAME " " FULL_VERSION_IDENTIFIER "...\n");
    
    // We need the early memory manager to be able to parse the command-line
    // parameters, so initialize that now.
    kmem_early_init(multiboot);
    
    // Parse the command-line parameters passed by the bootloader
    parse_boot_cmdline(multiboot, &gparam);
    
    // Initialize the kernel-mode logging functions
    klog_init(&gparam);
    
    // Now, load the kernel symbols from the information passed by the
    // bootloader
    ksym_load_kernel_symbols(multiboot);
    
    // Now pass control to the next stage of kernel initialization
    kernel_main2(&gparam);
}
