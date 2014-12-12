#include <drivers/serial.hpp>
#include <hwio.hpp>

namespace serial
{
    const uint16 DEFAULT_DIVISOR = 12; // We run at 9600 baud by default
    const uint8 DEFAULT_DATA_BITS = 0b11; // 8 bits per character
    const uint8 DEFAULT_STOP_BITS = 0b0; // 1 stop bit
    const uint8 DEFAULT_PARITY = 0b000; // No parity
    
    TTYSerial::TTYSerial(uint16 width, uint16 height)
        : port(0), width(width), height(height), out_buffer_pos(0)
    {
    }
    
    void TTYSerial::init(uint16 divisor, uint8 data_bits, uint8 stop_bits, uint8 parity, bool irq_enabled)
    {
        if (port == 0) return;
        
        init_port(port, divisor, data_bits, stop_bits, parity, irq_enabled);
    }
    
    void TTYSerial::flush()
    {
        if (port == 0) return;
        
        for (uint32 i = 0; i < out_buffer_pos; i++)
        {
            send_byte(port, out_buffer[i]);
        }
        
        out_buffer_pos = 0;
    }
    
    void TTYSerial::clear()
    {
        if (port == 0) return;
        
        for (uint32 i = 0; i < height; i++)
            *this << '\n';
    }
    
    TTY& TTYSerial::operator <<(char c)
    {
        if (port == 0) return *this;
        
        if (c == '\n')
        {
            flush();
            
            // Many serial terminals don't deal well with a simple LF. They need a
            // CRLF, so send that instead.
            send_byte(port, '\r');
            send_byte(port, '\n');
        }
        else
        {
            out_buffer[out_buffer_pos++] = c;
            
            if (out_buffer_pos == (sizeof(out_buffer) / sizeof(*out_buffer)))
                flush();
        }
        
        return *this;
    }
    
    TTYSerial com1(80, 24);
    TTYSerial com2(80, 24);
    TTYSerial com3(80, 24);
    TTYSerial com4(80, 24);
    
    void init()
    {
        // The serial I/O port numbers can be read from the BIOS data area as 4 16-bit
        // integers.
        uint16* ports = ((uint16*) 0xC0000400);
        
        // Set the ports for all of the serial TTYs
        com1.setPort(ports[0]);
        com2.setPort(ports[1]);
        com3.setPort(ports[2]);
        com4.setPort(ports[3]);
        
        // Initialize all of the serial ports to the default settings
        com1.init(DEFAULT_DIVISOR, DEFAULT_DATA_BITS, DEFAULT_STOP_BITS, DEFAULT_PARITY, false);
        com2.init(DEFAULT_DIVISOR, DEFAULT_DATA_BITS, DEFAULT_STOP_BITS, DEFAULT_PARITY, false);
        com3.init(DEFAULT_DIVISOR, DEFAULT_DATA_BITS, DEFAULT_STOP_BITS, DEFAULT_PARITY, false);
        com4.init(DEFAULT_DIVISOR, DEFAULT_DATA_BITS, DEFAULT_STOP_BITS, DEFAULT_PARITY, false);
    }
    
    void init_port(uint16 port, uint16 divisor, uint8 data_bits, uint8 stop_bits, uint8 parity, bool irq_enabled)
    {
        // Disable interrupts
        outb(port + 1, 0x00);
        
        // Set the divisor
        outb(port + 3, 0x80);
        outb(port + 0, divisor & 0xFF);
        outb(port + 1, divisor >> 8);
        
        // Set the number of data bits, number of stop bits, and parity type.
        outb(port + 3, (data_bits & 0b11) | ((stop_bits & 0b1) << 2) | ((parity & 0b111) << 3));
        
        // Enable FIFO
        outb(port + 2, 0xC7);
        
        // If IRQs were requested, enable them
        if (irq_enabled)
            outb(port + 4, 0x0B);
    }
    
    void send_byte(uint16 port, uint8 byte)
    {
        while ((inb(port + 5) & 0x20) == 0) ;
        
        outb(port, byte);
    }
}
