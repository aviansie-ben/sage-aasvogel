#include <core/ksym.h>

extern const void _ld_kernel_end;

bool ksym_address_lookup(uint32 virtual_address, const char** symbol_name, uint32* symbol_offset)
{
    if (virtual_address >= 0xC0000000 && virtual_address < ((uint32) &_ld_kernel_end + 0xC0000000))
    {
        *symbol_name = "kernel";
        *symbol_offset = (virtual_address - 0xC0000000);
        
        return true;
    }
    else
    {
        return false;
    }
}
