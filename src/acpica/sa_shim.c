#include "acpi.h"
#include <core/crash.h>
#include <core/klog.h>
#include <printf.h>

#include <memory/virt.h>
#include <memory/pool.h>
#include <core/sched.h>

#include <lock/mutex.h>
#include <lock/semaphore.h>
#include <lock/spinlock.h>

ACPI_STATUS AcpiOsInitialize(void)
{
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void)
{
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
    ACPI_PHYSICAL_ADDRESS Ret;

    AcpiFindRootPointer(&Ret);
    return Ret;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* InitVal, ACPI_STRING* NewVal)
{
    *NewVal = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_TABLE_HEADER** NewTable)
{
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_PHYSICAL_ADDRESS* NewAddress, UINT32* NewLength)
{
    *NewAddress = 0;
    *NewLength = 0;
    return AE_OK;
}

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    uint32 offset = 0;

    if ((Where & FRAME_OFFSET_MASK) != 0)
    {
        offset = (Where & FRAME_OFFSET_MASK);
        Length += offset;
    }

    if (Where < 0x100000) return (void*)(uint32)(Where + 0xC0000000);

    Length = (Length + FRAME_SIZE - 1) / FRAME_SIZE;
    addr_v address = (addr_v) kmem_virt_alloc(Length);

    for (uint32 i = 0; i < Length; i++)
    {
        if (!kmem_page_global_map(address + FRAME_SIZE * i, 0, false, Where + FRAME_SIZE * i))
            crash("Failed to map ACPI memory!");
    }

    kmem_page_flush_region(address, Length);
    return (void*) (address + offset);
}

void AcpiOsUnmapMemory(void* LogicalAddress, ACPI_SIZE Size)
{
    addr_v address = (addr_v) LogicalAddress;

    if ((address & FRAME_OFFSET_MASK) != 0)
    {
        Size += address & FRAME_OFFSET_MASK;
        address &= ~FRAME_OFFSET_MASK;
    }

    if (address >= 0xC0000000 && address < 0xC0100000) return;

    Size = (Size + FRAME_SIZE - 1) / FRAME_SIZE;

    for (uint32 i = 0; i < Size; i++)
        kmem_page_global_unmap(address + FRAME_SIZE * i, false);

    kmem_page_flush_region(address, Size);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void* LogicalAddress, ACPI_PHYSICAL_ADDRESS* PhysicalAddress)
{
    crash("Not implemented: AcpiOsGetPhysicalAddress");
}

void* AcpiOsAllocate(ACPI_SIZE Size)
{
    return kmem_pool_generic_alloc(Size, FA_EMERG | FA_32BIT);
}

void AcpiOsFree(void* Memory)
{
    kmem_pool_generic_free(Memory);
}

BOOLEAN AcpiOsReadable(void* Memory, ACPI_SIZE Length)
{
    crash("Not implemented: AcpiOsReadable");
}

BOOLEAN AcpiOsWritable(void* Memory, ACPI_SIZE Length)
{
    crash("Not implemented: AcpiOsWritable");
}

ACPI_THREAD_ID AcpiOsGetThreadId(void)
{
    return (ACPI_THREAD_ID)(uint32)sched_thread_current();
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void* Context)
{
    crash("Not implemented: AcpiOsExecute");
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    sched_sleep(Milliseconds);
}

void AcpiOsStall(UINT32 Microseconds)
{
    // TODO: Stall more accurately
    for (; Microseconds != 0; Microseconds--)
        asm volatile ("PAUSE"); // Prevents the busy loop from being optimized away
}

void AcpiOsWaitEventsComplete(void)
{
    crash("Not implemented: AcpiOsWaitEventsComplete");
}

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX* OutHandle)
{
    *OutHandle = AcpiOsAllocate(sizeof(mutex));

    if (*OutHandle == NULL) return AE_NO_MEMORY;

    mutex_init(*OutHandle);
    return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle)
{
    AcpiOsFree(Handle);
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Time)
{
    if (Time == 0xffffu)
    {
        mutex_acquire(Handle);
        return AE_OK;
    }
    else
    {
        // TODO: Implement timeouts
        return mutex_try_acquire(Handle) ? AE_OK : AE_TIME;
    }
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle)
{
    mutex_release(Handle);
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE* OutHandle)
{
    *OutHandle = AcpiOsAllocate(sizeof(semaphore));

    if (*OutHandle == NULL) return AE_NO_MEMORY;

    semaphore_init(*OutHandle, InitialUnits);
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    AcpiOsFree(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if (Timeout == 0xffffu)
    {
        for (UINT32 i = 0; i < Units; i++)
            semaphore_wait(Handle);
    }
    else
    {
        // TODO: Implement timeouts
        for (UINT32 i = 0; i < Units; i++)
        {
            if (!semaphore_try_wait(Handle))
            {
                for (UINT32 j = 0; j < i; j++)
                {
                    semaphore_signal(Handle);
                }

                return AE_TIME;
            }
        }
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    for (UINT32 i = 0; i < Units; i++)
        semaphore_signal(Handle);

    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle)
{
    *OutHandle = AcpiOsAllocate(sizeof(spinlock));

    if (*OutHandle == NULL) return AE_NO_MEMORY;

    spinlock_init(*OutHandle);
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    AcpiOsFree(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    spinlock_acquire(Handle);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    spinlock_release(Handle);
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void* Context)
{
    klog(KLOG_LEVEL_WARN, "AcpiOsInstallInterruptHandler is not supported!\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler)
{
    crash("Not implemented: AcpiOsRemoveInterruptHandler");
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64* Value, UINT32 Width)
{
    crash("Not implemented: AcpiOsReadMemory");
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    crash("Not implemented: AcpiOsWriteMemory");
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width)
{
    if (Width == 8)
    {
        UINT8 v;

        asm volatile ("inb %1, %0" : "=a" (v) : "dN" ((uint16)Address));
        *Value = v;
    }
    else if (Width == 16)
    {
        UINT16 v;

        asm volatile ("inw %1, %0" : "=a" (v) : "dN" ((uint16)Address));
        *Value = v;
    }
    else if (Width == 32)
    {
        UINT32 v;

        asm volatile ("inl %1, %0" : "=a" (v) : "dN" ((uint16)Address));
        *Value = v;
    }
    else
    {
        return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    if (Width == 8)
        asm volatile ("outb %1, %0" : : "dN" ((uint16)Address), "a" ((uint8)Value));
    else if (Width == 16)
        asm volatile ("outw %1, %0" : : "dN" ((uint16)Address), "a" ((uint16)Value));
    else if (Width == 32)
        asm volatile ("outl %1, %0" : : "dN" ((uint16)Address), "a" (Value));
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg, UINT64* Value, UINT32 Width)
{
    crash("Not implemented: AcpiOsReadPciConfiguration");
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
    crash("Not implemented: AcpiOsWritePciConfiguration");
}

static char printf_buf[256];
static uint32 printf_buf_pos;

#define ACPICA_KLOG_DEBUG "ACPI: "
#define ACPICA_KLOG_WARN "ACPI Warning: "
#define ACPICA_KLOG_ERR "ACPI Error: "
#define ACPICA_KLOG_CRIT "ACPI Exception: "

static int printf_to_buf(void* i, char c)
{
    if (c == '\n')
    {
        if (printf_buf_pos != 0)
        {
            printf_buf[printf_buf_pos] = '\0';
            printf_buf_pos = 0;

            char* msg = NULL;
            uint32 lvl;

            if (!strncmp(printf_buf, ACPICA_KLOG_DEBUG, strlen(ACPICA_KLOG_DEBUG)))
            {
#ifdef ACPICA_DEBUG
                msg = &printf_buf[strlen(ACPICA_KLOG_DEBUG)];
                lvl = KLOG_LEVEL_DEBUG;
#endif
            }
            else if (!strncmp(printf_buf, ACPICA_KLOG_WARN, strlen(ACPICA_KLOG_WARN)))
            {
                msg = &printf_buf[strlen(ACPICA_KLOG_WARN)];
                lvl = KLOG_LEVEL_WARN;
            }
            else if (!strncmp(printf_buf, ACPICA_KLOG_ERR, strlen(ACPICA_KLOG_ERR)))
            {
                msg = &printf_buf[strlen(ACPICA_KLOG_ERR)];
                lvl = KLOG_LEVEL_ERR;
            }
            else if (!strncmp(printf_buf, ACPICA_KLOG_CRIT, strlen(ACPICA_KLOG_CRIT)))
            {
                msg = &printf_buf[strlen(ACPICA_KLOG_CRIT)];
                lvl = KLOG_LEVEL_CRIT;
            }
            else
            {
                msg = printf_buf;
                lvl = KLOG_LEVEL_NOTICE;
            }

            if (msg != NULL)
                klog(lvl, "acpica: %s\n", msg);
        }
    }
    else if (printf_buf_pos != sizeof(printf_buf) - 1)
        printf_buf[printf_buf_pos++] = c;

    return E_SUCCESS;
}

void AcpiOsPrintf(const char* Format, ...)
{
    va_list vararg;

    va_start(vararg, Format);
    gprintf(Format, 0xffffffffu, NULL, printf_to_buf, vararg);
    va_end(vararg);
}

void AcpiOsVprintf(const char* Format, va_list Args)
{
    gprintf(Format, 0xffffffffu, NULL, printf_to_buf, Args);
}

void AcpiOsRedirectOutput(void* Destination)
{
    crash("Not implemented: AcpiOsRedirectOutput");
}

UINT64 AcpiOsGetTimer(void)
{
    crash("Not implemented: AcpiOsGetTimer");
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void* Info)
{
    crash("Not implemented: AcpiOsSignal");
}

ACPI_STATUS AcpiGetLine(char* Buffer, UINT32 BufferLength, UINT32* BytesRead)
{
    crash("Not implemented: AcpiGetLine");
}
