#include <core/acpi.h>
#include <core/crash.h>
#include <core/klog.h>
#include <acpica/acpi.h>
#include <hwio.h>

#include <printf.h>

void acpi_init(void)
{
    if (AcpiInitializeSubsystem() != AE_OK)
        crash("AcpiInitializeSubsystem failed");
    
    if (AcpiInitializeTables(NULL, 16, false) != AE_OK)
        crash("AcpiInitializeTables failed");
    
    if (AcpiLoadTables() != AE_OK)
        crash("AcpiLoadTables failed");
    
    if (AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION) != AE_OK)
        crash("AcpiEnableSubsystem failed");
    
    if (AcpiInitializeObjects(ACPI_FULL_INITIALIZATION) != AE_OK)
        crash("AcpiInitializeObjects failed");
    
    klog(KLOG_LEVEL_DEBUG, "ACPI subsystem initialized successfully\n");
}

void acpi_shutdown(void)
{
    if (AcpiEnterSleepStatePrep(5) != AE_OK)
        crash("ACPI shutdown failed");
    
    asm volatile ("cli");
    AcpiEnterSleepState(5);
    
    crash("ACPI shutdown failed");
}

void acpi_restart(void)
{
    asm volatile ("cli");
    
    AcpiReset();
    
    // TODO: Implement ACPI reset via PCI reset register
    // TODO: Reset using the PS/2 keyboard controller
    // TODO: Reset by triple faulting
    
    crash("ACPI restart failed");
}
