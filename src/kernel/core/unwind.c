#include <core/unwind.h>

void unchecked_unwind(unsigned int eip, void* ebp, frame_handler handler)
{
    while (ebp != 0)
    {
        (*handler)(eip, ebp);
        
        eip = *(((unsigned int*) ebp) + 1);
        ebp = *((void**) ebp);
    }
}

void unchecked_unwind_here(int skip_frames, frame_handler handler)
{
    void* ebp;
    
    asm volatile ("mov %%ebp, %0" : "=r" (ebp));
    
    while (skip_frames > 0)
    {
        ebp = *((void**) ebp);
        skip_frames--;
    }
    
    unchecked_unwind(*(((unsigned int*) ebp) + 1), *((void**) ebp), handler);
}
