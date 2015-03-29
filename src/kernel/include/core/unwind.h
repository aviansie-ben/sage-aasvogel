#ifndef CORE_UNWIND_H
#define CORE_UNWIND_H

typedef void (*frame_handler)(unsigned int eip, void* ebp);

void unchecked_unwind(unsigned int eip, void* ebp, frame_handler handler);
void unchecked_unwind_here(int skip_frames, frame_handler handler);

#endif
