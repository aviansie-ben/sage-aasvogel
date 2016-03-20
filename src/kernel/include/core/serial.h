#ifndef CORE_SERIAL_H
#define CORE_SERIAL_H

#define NUM_SERIAL_PORTS 4

#include <typedef.h>
#include <lock/spinlock.h>
#include <lock/condvar.h>

typedef struct
{
    spinlock lock;
    
    uint16 io_port;
    bool interrupts_enabled;
    
    char* recv_buf;
    int recv_buf_len;
    int recv_buf_maxlen;
    int recv_buf_head;
    int recv_buf_tail;
    cond_var_s recv_buf_ready;
    
    char* send_buf;
    int send_buf_len;
    int send_buf_maxlen;
    int send_buf_head;
    int send_buf_tail;
    cond_var_s send_buf_ready;
} serial_port;

extern serial_port serial_ports[NUM_SERIAL_PORTS];

extern void serial_init(void) __hidden;
extern void serial_init_interrupts(void) __hidden;
extern void serial_stop_interrupts(void) __hidden;

extern int serial_send(serial_port* port, const char* c, size_t s, mutex* m);
extern int serial_receive(serial_port* port, char* c, size_t s, mutex* m);

#endif
