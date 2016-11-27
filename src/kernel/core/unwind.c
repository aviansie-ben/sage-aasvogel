#include <core/unwind.h>
#include <string.h>

void unchecked_unwind(uint32 eip, void** ebp, uint32 max_frames, frame_handler handler)
{
    while (ebp != 0 && max_frames != 0)
    {
        (*handler)(eip, (void*)ebp);
        
        if (!memcpy_safe(&eip, ebp + 1, sizeof(eip)) || !memcpy_safe(&ebp, ebp, sizeof(ebp)))
            return;
        
        max_frames--;
    }
}

void unchecked_unwind_here(uint32 skip_frames, uint32 max_frames, frame_handler handler)
{
    uint32 eip;
    void** ebp;
    
    asm volatile ("mov %%ebp, %0" : "=r" (ebp));
    
    while (skip_frames != 0)
    {
        if (!memcpy_safe(&ebp, ebp, sizeof(ebp)))
            return;
        
        skip_frames--;
    }
    
    if (!memcpy_safe(&eip, ebp + 1, sizeof(eip)) || !memcpy_safe(&ebp, ebp, sizeof(ebp)))
        return;
    
    unchecked_unwind(eip, ebp, max_frames, handler);
}

uint32 get_caller_address(void)
{
    void* ebp;
    
    asm volatile ("mov %%ebp, %0" : "=r" (ebp));
    
    ebp = *((void**)ebp);
    ebp = *((void**)ebp);
    
    return *(((uint32*) ebp) + 1);
}
