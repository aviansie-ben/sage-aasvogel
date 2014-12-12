#ifndef __SERIAL_HPP__
#define __SERIAL_HPP__

#include <typedef.hpp>
#include <tty.hpp>

namespace serial
{
    using tty::TTY;
    using tty::TTYColor;
    
    class TTYSerial : public TTY
    {
    public:
        TTYSerial(uint16 width, uint16 height);
        
        void setPort(uint16 port) { this->port = port; }
        uint16 getPort() const { return port; }
        
        bool isPresent() const { return port != 0; }
        
        void setSize(uint16 width, uint16 height) { this->width = width; this->height = height; }
        
        void init(uint16 divisor, uint8 data_bits, uint8 stop_bits, uint8 parity, bool irq_enabled);
        
        virtual void setColor(TTYColor bg, TTYColor fg) { /* Do nothing */ }
        
        virtual uint16 getWidth() const { return width; }
        virtual uint16 getHeight() const { return height; }
        
        virtual void flush();
        virtual void clear();
        
        virtual TTY& operator <<(char c);
        
        using TTY::operator <<;
    private:
        // It doesn't make sense to copy a serial TTY...
        TTYSerial(const TTYSerial&);
        TTYSerial& operator =(const TTYSerial&);
        
        uint16 port;
        uint16 width, height;
        
        char out_buffer[80];
        uint8 out_buffer_pos;
    };
    
    extern TTYSerial com1;
    extern TTYSerial com2;
    extern TTYSerial com3;
    extern TTYSerial com4;
    
    extern void init();
    
    extern void init_port(uint16 port, uint16 divisor, uint8 data_bits, uint8 stop_bits, uint8 parity, bool irq_enabled);
    extern void send_byte(uint16 port, uint8 byte);
}

#endif
