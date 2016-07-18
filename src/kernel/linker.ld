OUTPUT_FORMAT("elf32-i386")
ENTRY(boot)

SECTIONS
{
    /* The kernel should be loaded at 1MiB to avoid overwriting anything
       important in lower memory. */
    . = 0x100000;
    
    /* Pre-initialization code/data goes in this section. Other sections have
       addresses that assume that the kernel has already been mapped to
       0xC0000000, which is done by pre-initialization. */
    _ld_setup_begin = .;
    .setup :
    {
        . = ALIGN(0x1000);
        *(.mb_header)
        *(.setup)
        
        . = ALIGN(0x1000);
        *(.setup_data)
        
        . = ALIGN(0x1000);
        *(.setup_pagedir)
    }
    
    /* Once memory manager initialization is done, this area will be
       reclaimed for use for some operations requiring physical addresses
       less than 16MiB. We need at least 128KiB of memory reserved for this
       purpose. */
    . = MAX(_ld_setup_begin + 0x20000, .);
    . = ALIGN(0x1000);
     _ld_setup_end = .;
    
    /* For all sections below here, addresses will assume that the kernel has
       been properly mapped at 0xC0000000 by the pre-initialization code. */
    . += 0xC0000000;
    
    .text : AT(ADDR(.text) - 0xC0000000)
    {
        _ld_text_begin = .;
        *(.text)
        _ld_text_end = .;
    }
    
    .rodata ALIGN(0x1000) : AT(ADDR(.rodata) - 0xC0000000)
    {
        _ld_rodata_begin = .;
        *(.rodata*)
        _ld_rodata_end = .;
    }
    
    .data ALIGN(0x1000) : AT(ADDR(.data) - 0xC0000000)
    {
        _ld_data_begin = .;
        *(.data)
        _ld_data_end = .;
    }
    
    .bss ALIGN(0x1000) : AT(ADDR(.bss) - 0xC0000000)
    {
        _ld_bss_begin = .;
        *(COMMON)
        *(.bss)
        _ld_bss_end = .;
    }
    
    /* This symbol marks the end of the kernel binary as a physical address.
       Anything after this point in memory is either a module loaded by GRUB or
       dynamically allocated memory, which is not mapped at boot time by the
       pre-initialization code. */
    _ld_kernel_end = . - 0xC0000000;
    
    /* Early in the boot process, some memory is required to perform basic
       dynamic allocation of important data structures before proper memory
       management is available. In order to allow this, we reserve 256KiB of
       memory for this purpose immediately after the kernel binary. */
    .kmalloc_early ALIGN(0x1000) : AT(ADDR(.kmalloc_early) - 0xC0000000)
    {
        _ld_kmalloc_early_begin = .;
        . += 0x40000;
        _ld_kmalloc_early_end = .;
    }
}
