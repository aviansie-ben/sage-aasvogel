#include <typedef.hpp>
#include <multiboot.hpp>

extern "C" void kernel_main(multiboot_info* mb_info)
{
    hang();
}
