#include <io/serial.h>
#include <cpu/idt.h>
#include <core/crash.h>
#include <memory/pool.h>
#include <hwio.h>

#define BDA_SERIAL_PORTS ((uint16*)0xC0000400)
#define RECV_BUF_SIZE 8192
#define SEND_BUF_SIZE 8192

#define DEFAULT_BAUD_DIVISOR (uint16)(0x000C) // Divisor for 9600 baud
#define DEFAULT_MODE (uint8)(0x03) // Mode is for 8N1

serial_port serial_ports[NUM_SERIAL_PORTS];

static bool serial_send_ready(serial_port* port)
{
    return (inb((uint16)(port->io_port + 5)) & 0x20) == 0x20;
}

static bool serial_receive_ready(serial_port* port)
{
    return (inb((uint16)(port->io_port + 5)) & 0x1) == 0x1;
}

static void serial_standard_sink(regs32* r, serial_port* p, char c)
{
    if (p->recv_buf_len < p->recv_buf_maxlen)
    {
        p->recv_buf[p->recv_buf_head++] = c;
        p->recv_buf_len++;

        if (p->recv_buf_head >= p->recv_buf_maxlen)
            p->recv_buf_head -= p->recv_buf_maxlen;
    }

    cond_var_s_broadcast(&p->recv_buf_ready);
}

static void serial_send_buffer(serial_port* p, bool wait)
{
    while (true)
    {
        while (serial_send_ready(p) && p->send_buf_len != 0)
        {
            outb(p->io_port, (uint8)p->send_buf[p->send_buf_head++]);
            p->send_buf_len--;

            if (p->send_buf_head >= p->send_buf_maxlen) p->send_buf_head -= p->send_buf_maxlen;
        }

        if (wait && p->send_buf_len > 0)
        {
            while (!serial_send_ready(p)) asm volatile ("pause");
        }
        else
        {
            break;
        }
    }
    cond_var_s_broadcast(&p->send_buf_ready);
}

static void serial_port_handle_interrupt(regs32* r, serial_port* port)
{
    if (port->io_port == 0) return;

    uint32 int_type = (inb((uint16)(port->io_port + 2)) >> 1) & 0x7;

    if (int_type == 0x2 || int_type == 0x6)
    {
        spinlock_acquire(&port->lock);
        while (serial_receive_ready(port))
            port->recv_sink(r, port, (char) inb(port->io_port));

        spinlock_release(&port->lock);
    }
    else if (int_type == 0x1)
    {
        spinlock_acquire(&port->lock);
        serial_send_buffer(port, false);
        spinlock_release(&port->lock);
    }
}

static void serial_interrupt_1_3(regs32* r)
{
    serial_port_handle_interrupt(r, &serial_ports[0]);
    serial_port_handle_interrupt(r, &serial_ports[2]);
}

static void serial_interrupt_2_4(regs32* r)
{
    serial_port_handle_interrupt(r, &serial_ports[1]);
    serial_port_handle_interrupt(r, &serial_ports[3]);
}

static void serial_send_char(serial_port* port, char c, mutex* m)
{
    while (port->send_buf_len == port->send_buf_maxlen) cond_var_s_wait(&port->send_buf_ready, m);

    if (port->send_buf_len == 0 && serial_send_ready(port))
    {
        outb(port->io_port, (uint8)c);
    }
    else
    {
        port->send_buf[port->send_buf_tail++] = c;
        port->send_buf_len++;

        if (port->send_buf_tail >= port->send_buf_maxlen) port->send_buf_tail -= port->send_buf_maxlen;
    }
}

static void serial_send_char_immediate(serial_port* port, char c)
{
    while (!serial_send_ready(port)) asm volatile ("pause");
    outb(port->io_port, (uint8)c);
}

static void serial_port_disable_interrupts(uint16 io_port)
{
    outb((uint16)(io_port + 1), 0x00);
}

static void serial_port_enable_interrupts(uint16 io_port)
{
    outb((uint16)(io_port + 1), 0x03);
}

static void serial_port_send_config(uint16 io_port, uint16 divisor, uint8 mode)
{
    // Write the new divisor
    outb((uint16)(io_port + 3), 0x80);
    outb((uint16)(io_port + 0), (uint8)(divisor & 0xff));
    outb((uint16)(io_port + 1), (uint8)((divisor >> 8) & 0xff));

    // Write the new mode
    outb((uint16)(io_port + 3), mode);

    // Clear and enable FIFOs
    outb((uint16)(io_port + 2), 0xC7);

    // Enable DTR, RTS, and OUT2 in the MCR
    outb((uint16)(io_port + 4), 0x0B);
}

static void serial_port_flush_immediate(serial_port* port)
{
    while (port->send_buf_len != 0)
    {
        serial_send_char_immediate(port, port->send_buf[port->send_buf_head++]);
        port->send_buf_len--;

        if (port->send_buf_head >= port->send_buf_maxlen) port->send_buf_head -= port->send_buf_maxlen;
    }
}

static void serial_port_init(serial_port* port, uint16 io_port)
{
    port->io_port = io_port;
    port->recv_buf = NULL;
    port->send_buf = NULL;

    port->recv_sink = serial_standard_sink;

    if (io_port != 0)
    {
        serial_port_disable_interrupts(io_port);
        serial_port_send_config(io_port, DEFAULT_BAUD_DIVISOR, DEFAULT_MODE);
    }
}

static void serial_port_init_interrupts(serial_port* port)
{
    if (port->io_port != 0)
    {
        port->recv_buf = kmem_pool_generic_alloc(RECV_BUF_SIZE, 0);
        if (port->recv_buf == NULL)
            crash("Failed to allocate serial receive buffer");

        port->recv_buf_len = 0;
        port->recv_buf_maxlen = RECV_BUF_SIZE;
        port->recv_buf_head = 0;
        port->recv_buf_tail = 0;
        cond_var_s_init(&port->recv_buf_ready, &port->lock);

        port->send_buf = kmem_pool_generic_alloc(SEND_BUF_SIZE, 0);
        if (port->send_buf == NULL)
            crash("Failed to allocate serial send buffer");

        port->send_buf_len = 0;
        port->send_buf_maxlen = SEND_BUF_SIZE;
        port->send_buf_head = 0;
        port->send_buf_tail = 0;
        cond_var_s_init(&port->send_buf_ready, &port->lock);

        serial_port_enable_interrupts(port->io_port);
    }
}

static void serial_port_stop_interrupts(serial_port* port)
{
    if (port->io_port != 0) serial_port_disable_interrupts(port->io_port);

    if (port->recv_buf != NULL)
    {
        kmem_pool_generic_free(port->recv_buf);
        port->recv_buf = NULL;
    }

    if (port->send_buf != NULL)
    {
        serial_port_flush_immediate(port);
        kmem_pool_generic_free(port->send_buf);
        port->send_buf = NULL;
    }
}

void serial_init(void)
{
    for (uint32 i = 0; i < NUM_SERIAL_PORTS; i++)
        serial_port_init(&serial_ports[i], BDA_SERIAL_PORTS[i]);
}

void serial_init_interrupts(void)
{
    idt_register_irq_handler(3, serial_interrupt_2_4);
    idt_register_irq_handler(4, serial_interrupt_1_3);

    idt_set_irq_enabled(3, true);
    idt_set_irq_enabled(4, true);

    for (uint32 i = 0; i < NUM_SERIAL_PORTS; i++)
        serial_port_init_interrupts(&serial_ports[i]);
}

void serial_stop_interrupts(void)
{
    for (uint32 i = 0; i < NUM_SERIAL_PORTS; i++)
        serial_port_stop_interrupts(&serial_ports[i]);
}

void serial_set_sink(serial_port* port, serial_sink_function fn)
{
    if (!fn)
        fn = serial_standard_sink;

    port->recv_sink = fn;
}

int serial_send_spin(serial_port* port, const char* c, size_t s)
{
    if (port->io_port == 0) return E_IO_ERROR;

    if (port->send_buf != NULL && port->send_buf_len != 0)
        serial_send_buffer(port, true);

    while (s != 0)
    {
        serial_send_char_immediate(port, *(c++));
        s--;
    }

    return E_SUCCESS;
}

int serial_send(serial_port* port, const char* c, size_t s, mutex* m)
{
    if (port->send_buf == NULL) return serial_send_spin(port, c, s);
    if (port->io_port == 0) return E_IO_ERROR;

    while (s != 0)
    {
        serial_send_char(port, *(c++), m);
        s--;
    }

    return E_SUCCESS;
}

int serial_receive_spin(serial_port* port, char* c, size_t s)
{
    if (port->io_port == 0) return E_IO_ERROR;

    if (port->recv_buf != NULL)
    {
        while (s != 0 && port->recv_buf_len != 0)
        {
            *(c++) = port->recv_buf[port->recv_buf_tail++];
            port->recv_buf_len--;
            s--;

            if (port->recv_buf_tail >= port->recv_buf_maxlen) port->recv_buf_tail -= port->recv_buf_maxlen;
        }
    }

    while (s != 0)
    {
        while (!serial_receive_ready(port)) asm volatile ("pause");

        *(c++) = (char) inb(port->io_port);
        s--;
    }

    return E_SUCCESS;
}

int serial_receive(serial_port* port, char* c, size_t s, mutex* m)
{
    if (port->recv_buf == NULL) return serial_receive_spin(port, c, s);
    if (port->io_port == 0) return E_IO_ERROR;

    while (s != 0)
    {
        while (port->recv_buf_len == 0) cond_var_s_wait(&port->recv_buf_ready, m);

        while (s != 0 && port->recv_buf_len != 0)
        {
            *(c++) = port->recv_buf[port->recv_buf_tail++];
            port->recv_buf_len--;
            s--;

            if (port->recv_buf_tail >= port->recv_buf_maxlen) port->recv_buf_tail -= port->recv_buf_maxlen;
        }
    }

    return E_SUCCESS;
}
