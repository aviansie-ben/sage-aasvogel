#include <typedef.hpp>

extern "C" int __cxa_atexit(void (*destructor)(void*), void* arg, void* __dso_handle)
{
    // Do nothing
    return 0;
}

extern "C" void __cxa_pure_virtual()
{
    // Do nothing
}
