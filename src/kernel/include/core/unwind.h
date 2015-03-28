#ifndef __UNWIND_H__
#define __UNWIND_H__

typedef void (*frame_handler)(unsigned int eip, void* ebp);

void unchecked_unwind(unsigned int eip, void* ebp, frame_handler handler);
void unchecked_unwind_here(int skip_frames, frame_handler handler);

#endif
