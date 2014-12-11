#include <typedef.hpp>
#include <memory/mem.hpp>

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
    return mem::kmalloc(size, 0, 0);
}

void* operator new(size_t size, void* ptr)
{
    return ptr;
}

void* operator new[](size_t size) throw()
{
    return mem::kmalloc(size, 0, 0);
}

void* operator new[](size_t size, void* ptr)
{
    return ptr;
}

void operator delete(void* ptr)
{
    mem::kfree(ptr);
}

void operator delete[](void* ptr)
{
    mem::kfree(ptr);
}
