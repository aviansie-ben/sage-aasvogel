#ifndef CORE_GDB_STUB_H
#define CORE_GDB_STUB_H

#include <typedef.h>
#include <core/bootparam.h>

#define STOPPED_INTERRUPTED 0
#define STOPPED_BREAKPOINT  1
#define STOPPED_ABORT       2
#define STOPPED_PAGE_FAULT  3

extern void gdb_stub_init(const boot_param* param) __hidden;
extern bool gdb_stub_is_active(void);

extern void gdb_stub_break(int reason);
extern void gdb_stub_break_interrupt(regs32* r, int reason);

#endif
