#ifndef CORE_UNWIND_H
#define CORE_UNWIND_H

#include <typedef.h>

typedef void (*frame_handler)(unsigned int eip, void* ebp);

void unchecked_unwind(uint32 eip, void** ebp, uint32 max_frames, frame_handler handler);
void unchecked_unwind_here(uint32 skip_frames, uint32 max_frames, frame_handler handler);

uint32 get_caller_address(void);

#endif
