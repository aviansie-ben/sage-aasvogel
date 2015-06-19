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

#include <core/sched.h>

static boot_param gparam;

void kernel_main(multiboot_info* multiboot);

static void sched_test_2(void* id)
{
    int i;
    int j;
    
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3000000; j++) ;
        
        spinlock_acquire(&tty_virtual_consoles[1].base.lock);
        tprintf(&tty_virtual_consoles[1].base, "Hello %d from thread %d! (%ld)\n", i + 1, (int)id + 1, sched_thread_current()->tid);
        klog(KLOG_LEVEL_DEBUG, "Hello %d from thread %d! (%ld)\n", i + 1, (int)id + 1, sched_thread_current()->tid);
        spinlock_release(&tty_virtual_consoles[1].base.lock);
    }
    
    spinlock_acquire(&sched_process_current()->lock);
    sched_thread_destroy(sched_thread_current());
    spinlock_release(&sched_process_current()->lock);
    sched_yield();
}

static void sched_test_1(void)
{
    sched_process* p = sched_process_current();
    sched_thread* t;
    
    tty_switch_vc(&tty_virtual_consoles[1]);
    
    spinlock_acquire(&p->lock);
    for (int i = 0; i < 10; i++)
    {
        sched_thread_create(p, sched_test_2, (void*)i, &t);
    }
    sched_thread_current()->status = STS_DEAD;
    spinlock_release(&p->lock);
    
    sched_yield();
}

static void kernel_main2(const boot_param* param)
{
    // Run CPUID checks
    cpuid_init();
    klog(KLOG_LEVEL_INFO, "Detected CPU Vendor: %s (%s)\n", cpuid_detected_vendor->vendor_name, cpuid_detected_vendor->vendor_id.str);
    
    // Initialize the GDT and IDT
    gdt_init();
    idt_init();
    
    // Initialize the memory manager
    // TODO: Fix memory management
    /* kmem_phys_init(param);
    kmem_page_init(param); */
    
    // Initialize the CPU scheduler
    sched_init(param);
    sched_test_1();
    
    hang();
}

void kernel_main(multiboot_info* multiboot)
{
    // Initialize basic console I/O
    console_init();
    tty_init();
    
    tprintf(&tty_virtual_consoles[0].base, "%CfBooting " OS_NAME " v" STRINGIFY(MAJOR_VERSION) "." STRINGIFY(MINOR_VERSION) "...\n", CONSOLE_COLOR_LIGHT_GRAY);
    
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
