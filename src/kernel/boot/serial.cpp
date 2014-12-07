/*
 * This file contains pre-initialization code. This code is mapped into the
 * .setup section and is generally executed BEFORE the kernel has been properly
 * mapped at 0xC0000000. Thus, the use of ANY functions or variables outside of
 * this file SHOULD NOT BE ASSUMED TO WORK PROPERLY!
 */

#include "preinit.hpp"

static uint16 _preinit_serial_port __section__(".setup_data") = 0;

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

extern "C" void _preinit_setup_serial()
{
    if (!_preinit_serial_enable) return;
    
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
