#ifndef CORE_KSYM_H
#define CORE_KSYM_H

#include <typedef.h>

extern bool ksym_address_lookup(uint32 virtual_address, const char** symbol_name, uint32* symbol_offset);
extern uint32 ksym_symbol_lookup(const char* symbol_name);

#endif
