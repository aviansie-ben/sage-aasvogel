#include <typedef.hpp>
#include <multiboot.hpp>

#include <core/gdt.hpp>

extern "C" void kernel_main(multiboot_info* mb_info)
{
    gdt::init();
    hang();
}
