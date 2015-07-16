#ifndef CORE_MSR_H
#define CORE_MSR_H
#include <typedef.h>

#define MSR_EFER 0xC0000080
#define MSR_EFER_FLAG_NX (1 << 11)

extern bool msr_is_supported(void) __const;

extern uint64 msr_read(uint32 msr) __pure;
extern void msr_write(uint32 msr, uint64 val);

#endif
