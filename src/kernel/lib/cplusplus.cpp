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

void* operator new(size_t size) throw()
{
    // TODO: Implement this
    return 0;
}

void* operator new(size_t size, void* ptr)
{
    return ptr;
}

void* operator new[](size_t size) throw()
{
    // TODO: Implement this
    return 0;
}

void* operator new[](size_t size, void* ptr)
{
    return ptr;
}

void operator delete(void* ptr)
{
    // TODO: Implement this
}

void operator delete[](void* ptr)
{
    // TODO: Implement this
}
