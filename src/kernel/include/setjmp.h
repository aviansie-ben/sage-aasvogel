#ifndef SETJMP_H
#define SETJMP_H

#include <typedef.h>

struct __jmp_buf_tag
{
    uint32 eip;
    
    uint32 esp;
    uint32 ebp;
    
    uint32 ebx;
    uint32 edi;
    uint32 esi;
};

typedef struct __jmp_buf_tag jmp_buf[1];

extern int setjmp(jmp_buf env);
extern void longjmp(jmp_buf env, int val) __attribute__((noreturn));
extern void longjmp_interrupt(jmp_buf env, int val, regs32* r);

#endif
