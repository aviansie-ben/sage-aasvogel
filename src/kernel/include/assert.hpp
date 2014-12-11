#ifndef __ASSERT_HPP__
#define __ASSERT_HPP__

#include <core/crash.hpp>

#define assert_always(cond) do { if (!(cond)) { crash("Assertion failed: " #cond); } } while (0)

#ifndef DISABLE_ASSERT
    #define assert(cond) assert_always(cond)
#else
    #define assert(cond) ((void)sizeof(cond))
#endif

#endif
