/*
 * This file contains pre-initialization code. This code is mapped into the
 * .setup section and is generally executed BEFORE the kernel has been properly
 * mapped at 0xC0000000. Thus, the use of ANY functions or variables outside of
 * this file SHOULD NOT BE ASSUMED TO WORK PROPERLY!
 */

#include "preinit.h"

static uint16 _preinit_serial_port __section__(".setup_data") = 0;

void _preinit_outb(uint16 port, uint8 data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

uint8 _preinit_inb(uint16 port)
{
    uint8 data;
    asm volatile ("inb %1, %0" : "=a" (data) : "dN" (port));
    return data;
}

void _preinit_setup_serial()
{
    if (!_preinit_serial_enable) return;
    
    _preinit_serial_port = *((uint16*) 0x400);
    
    // Disable interrupts (We haven't set up an IDT yet!)
    _preinit_outb((uint16)(_preinit_serial_port + 1), 0x00);
    
    // Set the divisor to 12 (9600 baud)
    _preinit_outb((uint16)(_preinit_serial_port + 3), 0x80);
    _preinit_outb((uint16)(_preinit_serial_port + 0), 0x0C);
    _preinit_outb((uint16)(_preinit_serial_port + 1), 0x00);
    
    // Set the mode to 8N1
    _preinit_outb((uint16)(_preinit_serial_port + 3), 0x03);
    
    // Enable FIFO
    _preinit_outb((uint16)(_preinit_serial_port + 2), 0xC7);
    
    _preinit_write_serial(_preinit_serial_init_msg);
}

void _preinit_write_serial(const char* message)
{
    size_t i;
    
    if (_preinit_serial_port == 0) return;
    
    for (i = 0; message[i] != '\0'; i++)
    {
        while ((_preinit_inb((uint16)(_preinit_serial_port + 5)) & 0x20) == 0) ;
        _preinit_outb(_preinit_serial_port, (uint8)message[i]);
    }
}
