#ifndef HWIO_H
#define HWIO_H

#include <typedef.h>

static inline void outb(uint16 port, uint8 data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

static inline uint8 inb(uint16 port)
{
    uint8 data;
    asm volatile ("inb %1, %0" : "=a" (data) : "dN" (port));
    return data;
}

static inline void io_wait(void)
{
    // Running OUTB causes the CPU to wait for an I/O operation to complete. We
    // use port 0x80 here, since it is almost always an unused port.
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif
