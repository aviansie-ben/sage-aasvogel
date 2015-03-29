#ifndef ASSERT_H
#define ASSERT_H

#include <core/crash.h>

#define assert(cond) do { if (!(cond)) { crash("Assertion failed: " #cond); } } while(0)

#endif
