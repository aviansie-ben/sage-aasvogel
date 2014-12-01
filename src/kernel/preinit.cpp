/*
 * This file contains pre-initialization code. This code is mapped into the
 * .setup section and is generally executed BEFORE the kernel has been properly
 * mapped at 0xC0000000. Thus, the use of ANY functions or variables outside of
 * this file SHOULD NOT BE ASSUMED TO WORK PROPERLY!
 */

#include <typedef.hpp>

#define __section__(s) __attribute__((section(s)))

// External symbols added by the linker to tell us where the different sections
// are located.
extern uint8 _ld_kernel_end[];
extern uint8 _ld_rodata_begin[];
extern uint8 _ld_rodata_end[];
extern uint8 _ld_text_begin[];
extern uint8 _ld_text_end[]; 

const uint8 _preinit_error_color = 0x04;

// All PDEs and PTEs need to have the present bit set, and all non-read-only
// PTEs need to have the writable bit set.
const uint32 _preinit_pdpte_flags      = 0b000000001;
const uint32 _preinit_pde_flags        = 0b000000011;
const uint32 _preinit_pte_flags        = 0b000000011;
const uint32 _preinit_pte_flags_rodata = 0b000000001;
const uint32 _preinit_pte_flags_text   = 0b000000001;

typedef union
{
    uint32 legacy[1024];
    uint64 pae[512];
} page_struct_t;

char _preinit_serial_stop_msg[] __section__(".setup_data") = "\r\nKernel pre-initialization failed! Hanging...\r\n";
char _preinit_serial_init_msg[] __section__(".setup_data") = "Serial port initialized!\r\nPre-initialization in progress...\r\n";
char _preinit_serial_page_legacy_msg[] __section__(".setup_data") = "Setting up pre-initialization paging without PAE support...\r\n";
char _preinit_serial_page_pae_msg[] __section__(".setup_data") = "Setting up pre-initialization paging with PAE support...\r\n";

char _preinit_pae_unsupported_error[] __section__(".setup_data") = "FATAL: CPU does not support PAE!";
char _preinit_kernel_too_big_error[] __section__(".setup_data") = "FATAL: Kernel is too big for statically allocated page tables!";
uint16 _preinit_serial_port __section__(".setup_data");

page_struct_t _preinit_page_dir __attribute__((section(".setup_pagedir"), aligned(0x1000)));
page_struct_t _preinit_page_table __attribute__((section(".setup_pagedir"), aligned(0x1000)));
uint64 _preinit_page_dir_ptr_table[4] __attribute__((section(".setup_pagedir"), aligned(0x20)));

extern "C" void _preinit_outb(uint16 port, uint8 data) __section__(".setup");
extern "C" uint8 _preinit_inb(uint16 port) __section__(".setup");

extern "C" void _preinit_error(const char* message) __attribute__((section(".setup"), noreturn));
extern "C" void _preinit_setup_serial() __section__(".setup");
extern "C" void _preinit_write_serial(const char* message) __section__(".setup");
extern "C" void* _preinit_setup_paging(bool pae_supported) __section__(".setup");
extern "C" void* _preinit_setup_paging_pae() __section__(".setup");
extern "C" void* _preinit_setup_paging_legacy() __section__(".setup");

extern "C" void _preinit_outb(uint16 port, uint8 data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

extern "C" uint8 _preinit_inb(uint16 port)
{
    uint8 data;
    asm volatile ("inb %1, %0" : "=a" (data) : "dN" (port));
    return data;
}

extern "C" void _preinit_error(const char* message)
{
    // Pointer into the standard location for writing characters to the console
    uint8* video = (uint8*) 0xB8000;
    
    // Each character is printed individually, and the console color is set for each
    // printed character.
    for (size_t i = 0; message[i] != '\0'; i++)
    {
        video[i * 2] = message[i];
        video[(i * 2) + 1] = _preinit_error_color;
    }
    
    // If possible, write the error message out to the serial port as well
    _preinit_write_serial(message);
    _preinit_write_serial(_preinit_serial_stop_msg);
    
    // We have crashed, so hang the CPU
    hang();
}

extern "C" void _preinit_setup_serial()
{
    _preinit_serial_port = *((uint16*) 0x400);
    
    // Disable interrupts (We haven't set up an IDT yet!)
    _preinit_outb(_preinit_serial_port + 1, 0x00);
    
    // Set the divisor to 3 (38400 baud)
    _preinit_outb(_preinit_serial_port + 3, 0x80);
    _preinit_outb(_preinit_serial_port + 0, 0x03);
    _preinit_outb(_preinit_serial_port + 1, 0x00);
    
    // Set the mode to 8N1
    _preinit_outb(_preinit_serial_port + 3, 0x03);
    
    // Enable FIFO
    _preinit_outb(_preinit_serial_port + 2, 0xC7);
    
    _preinit_write_serial(_preinit_serial_init_msg);
}

extern "C" void _preinit_write_serial(const char* message)
{
    if (_preinit_serial_port == 0) return;
    
    for (size_t i = 0; message[i] != '\0'; i++)
    {
        while ((_preinit_inb(_preinit_serial_port + 5) & 0x20) == 0) ;
        _preinit_outb(_preinit_serial_port, message[i]);
    }
}

extern "C" void* _preinit_setup_paging(bool pae_supported)
{
    // TODO: Add support for non-PAE systems
    if (pae_supported) return _preinit_setup_paging_pae();
    else return _preinit_setup_paging_legacy();
}

extern "C" void* _preinit_setup_paging_pae()
{
    _preinit_write_serial(_preinit_serial_page_pae_msg);
    
    // Get the PDPT and PD entry values to use
    uint64 pdpte = (((uint32)&_preinit_page_dir) & 0xFFFFF000) | _preinit_pdpte_flags;
    uint64 pde = (((uint32)&_preinit_page_table) & 0xFFFFF000) | _preinit_pde_flags;
    
    // We will be mapping the kernel in at both 0x00000000 and 0xC0000000, so we can
    // use the same page directory for both locations.
    _preinit_page_dir_ptr_table[0] = _preinit_page_dir_ptr_table[3] = pdpte;
    
    // Map the page table into the page directory.
    _preinit_page_dir.pae[0] = pde;
    
    // Now we set the page table entries to map everything from 0x00000000 to the end
    // of the kernel binary.
    for (uint32 i = 0; (i * 0x1000) < (uint32)&_ld_kernel_end; i++)
    {
        // We only allocated one page table in the .setup section... If the kernel is
        // too big for this, we'll need to add more before compiling.
        if (i >= 512) _preinit_error(_preinit_kernel_too_big_error);
        
        uint32 phys_addr = (i * 0x1000);
        uint32 virt_addr = phys_addr + 0xC0000000;
        uint32 flags;
        
        // Pages in the .rodata and .text section have different flags applied to them
        if (virt_addr >= (uint32)&_ld_rodata_begin && virt_addr < (uint32)&_ld_rodata_end)
            flags = _preinit_pte_flags_rodata;
        else if (virt_addr >= (uint32)&_ld_text_begin && virt_addr < (uint32)&_ld_text_end)
            flags = _preinit_pte_flags_text;
        else
            flags = _preinit_pte_flags;
        
        _preinit_page_table.pae[i] = (phys_addr & 0xFFFFF000) | flags;
    }
    
    return &_preinit_page_dir_ptr_table;
}

extern "C" void* _preinit_setup_paging_legacy()
{
    _preinit_write_serial(_preinit_serial_page_legacy_msg);
    
    // Get the PD entry value to use
    uint32 pde = (((uint32)&_preinit_page_table) & 0xFFFFF000) | _preinit_pde_flags;
    
    // We will be mapping the kernel in at both 0x00000000 and 0xC0000000, so we can
    // use the same page table for both locations.
    _preinit_page_dir.legacy[0x000] = _preinit_page_dir.legacy[0x300] = pde;
    
    // Now we set the page table entries to map everything from 0x00000000 to the end
    // of the kernel binary.
    for (uint32 i = 0; (i * 0x1000) < (uint32)&_ld_kernel_end; i++)
    {
        // We only allocated one page table in the .setup section... If the kernel is
        // too big for this, we'll need to add more before compiling.
        if (i >= 1024) _preinit_error(_preinit_kernel_too_big_error);
        
        uint32 phys_addr = (i * 0x1000);
        uint32 virt_addr = phys_addr + 0xC0000000;
        uint32 flags;
        
        // Pages in the .rodata and .text section have different flags applied to them
        if (virt_addr >= (uint32)&_ld_rodata_begin && virt_addr < (uint32)&_ld_rodata_end)
            flags = _preinit_pte_flags_rodata;
        else if (virt_addr >= (uint32)&_ld_text_begin && virt_addr < (uint32)&_ld_text_end)
            flags = _preinit_pte_flags_text;
        else
            flags = _preinit_pte_flags;
        
        _preinit_page_table.legacy[i] = (phys_addr & 0xFFFFF000) | flags;
    }
    
    return &_preinit_page_dir;
}
