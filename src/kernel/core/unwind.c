#include <core/unwind.h>

void unchecked_unwind(uint32 eip, void* ebp, uint32 max_frames, frame_handler handler)
{
    while (ebp != 0 && max_frames != 0)
    {
        (*handler)(eip, ebp);
        
        eip = *(((unsigned int*) ebp) + 1);
        ebp = *((void**) ebp);
        max_frames--;
    }
}

void unchecked_unwind_here(uint32 skip_frames, uint32 max_frames, frame_handler handler)
{
    void* ebp;
    
    asm volatile ("mov %%ebp, %0" : "=r" (ebp));
    
    while (skip_frames != 0)
    {
        ebp = *((void**) ebp);
        skip_frames--;
    }
    
    unchecked_unwind(*(((unsigned int*) ebp) + 1), *((void**) ebp), max_frames, handler);
}
