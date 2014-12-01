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

#endif
