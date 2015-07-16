#ifndef CORE_KSYM_H
#define CORE_KSYM_H

#include <typedef.h>
#include <multiboot.h>

typedef enum
{
    KSYM_ALOOKUP_RET = (1 << 0)
} ksym_address_lookup_flags;

typedef enum
{
    KSYM_TYPE_FUNCTION,
    KSYM_TYPE_OBJECT,
    KSYM_TYPE_OTHER
} kernel_symbol_type;

typedef enum
{
    KSYM_VISIBILITY_PUBLIC,
    KSYM_VISIBILITY_PRIVATE,
    KSYM_VISIBILITY_FILE_LOCAL
} kernel_symbol_visibility;

typedef struct
{
    const char* name;
    uint32 address;
    uint32 size;
    
    uint8 type;
    uint8 visibility;
} kernel_symbol;

extern const kernel_symbol* ksym_address_lookup(uint32 virtual_address, uint32* symbol_offset, uint32 flags) __pure;
extern uint32 ksym_symbol_lookup(const char* symbol_name) __pure;

extern void ksym_load_kernel_symbols(multiboot_info* multiboot) __hidden;

#endif
