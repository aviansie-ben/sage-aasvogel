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
    .setup :
    {
        _ld_setup_begin = .;
        . = ALIGN(0x1000);
        *(.mb_header)
        *(.setup)
        
        . = ALIGN(0x1000);
        *(.setup_data)
        
        . = ALIGN(0x1000);
        *(.setup_pagedir)
        _ld_setup_end = .;
    }
    
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
        *(.rodata)
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
}
