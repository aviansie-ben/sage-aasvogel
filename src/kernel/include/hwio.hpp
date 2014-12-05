#ifndef __HWIO_HPP__
#define __HWIO_HPP__

#include <typedef.hpp>

inline void outb(uint16 port, uint8 data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

inline uint8 inb(uint16 port)
{
    uint8 data;
    asm volatile ("inb %1, %0" : "=a" (data) : "dN" (port));
    return data;
}

inline void io_wait()
{
    // Running OUTB causes the CPU to wait for an I/O operation to complete. We use port 0x80
    // here, since it is almost always an unused port.
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif
