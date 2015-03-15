#ifndef __ASSERT_H__
#define __ASSERT_H__

#include <core/crash.h>

#define assert(cond) do { if (!(cond)) { crash("Assertion failed: " #cond); } } while(0)

#endif
