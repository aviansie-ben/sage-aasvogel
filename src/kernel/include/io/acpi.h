#ifndef CORE_ACPI_H
#define CORE_ACPI_H

#include <typedef.h>

extern void acpi_init(void) __hidden;

extern void acpi_shutdown(void) __attribute__((noreturn));
extern void acpi_restart(void) __attribute__((noreturn));

#endif
