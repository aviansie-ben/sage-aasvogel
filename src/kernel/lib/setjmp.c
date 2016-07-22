#include <setjmp.h>

void longjmp_interrupt(jmp_buf env, int val, regs32* r)
{
    r->eax = (uint32)val;
    
    r->eip = env->eip;
    
    // Since we're not going to be executing the final "ret" instruction from longjmp, we need to
    // skip the word on the stack that would normally be used to store the return address for that
    // instruction.
    r->esp = env->esp + 4;
    r->ebp = env->ebp;
    
    r->ebx = env->ebx;
    r->edi = env->edi;
    r->esi = env->esi;
}
