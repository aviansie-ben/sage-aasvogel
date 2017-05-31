.intel_syntax noprefix
.altmacro

#include <cpu/idt.h>

.section .text
.globl idt_flush
.type idt_flush, @function
idt_flush:
    # Get the pointer that we have been passed to the structure that points to
    # the IDT
    mov eax, [esp + 0x4]

    # Request that the CPU load the IDT pointer we have received
    lidt [eax]

    ret
.size idt_flush, .-idt_flush

.type interrupt_common, @function
interrupt_common:
    # Push registers onto the stack
    pusha
    push ds
    push es
    push fs
    push gs

    # Load the segment registers for kernel data
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    # If we end up printing a stack trace, we don't want anything below this
    # frame to be printed, so put some marker values on the stack.
    mov edx, esp
    push 0
    push 0
    mov ebp, esp
    push edx

    # Check whether the code we were in before the interrupt was ring 0 code.
    # Ring 0 code calling into an interrupt does not push useresp or ss, so we
    # need to make 8 bytes of room to store these values to avoid clobbering
    # part of the stack if we return into ring 3.

    push edx
    call _idt_is_ring0
    add esp, 4
    cmp eax, 0
    je .Ldo_call

    sub dword ptr [esp], 8
    sub esp, 8
    sub ebp, 8
    mov eax, esp
    mov ebx, eax
    add ebx, 0x50

.Lalloc_ring0:
    mov ecx, dword ptr [eax + 0x8]
    mov dword ptr [eax], ecx
    add eax, 4

    cmp eax, ebx
    jl .Lalloc_ring0

    # Find the stack pointer that the ring 0 code was using before the interrupt
    # was called.
    mov [ebx], esp
    add dword ptr [ebx], 0x58

    # Find the ring 0 code's SS value.
    mov eax, ss
    mov [ebx + 0x4], eax

.Ldo_call:
    # Call the C interrupt handler
    mov edx, [esp]
    push edx
    call _idt_handle
    add esp, 4

    # Now we need to check whether the code we're returning to is ring 0 code.
    # If it is, then the ESP and SS values will not get popped by IRET, so we
    # need to pop them manually.
    mov edx, [esp]
    push edx
    call _idt_is_ring0
    add esp, 4

    # Get rid of the EBP marker values on the stack, as we don't need them after
    # this.
    mov edx, [esp]
    add esp, 0xC

    cmp eax, 0
    je .Lreturn

    # Pop the SS value that we should switch to.
    mov eax, [edx + 0x48]
    mov ss, eax

    # First, we find the location of the original regs32 structure, as well as
    # the new location it should be moved to in order to return with the correct
    # ESP value.
    mov esi, esp
    mov edi, [edx + 0x44]
    sub edi, 0x44

    # Next, we need to make sure that the stack pointer is lower than all of the
    # memory we will be manipulating. If it's not, parts of the stack used for
    # the call to memmove might be overwritten during the copy.
    cmp esi, edi
    jb .Lsource_low

    mov esp, edi
    jmp .Lfinish_esp_ring0

.Lsource_low:
    mov esp, esi

.Lfinish_esp_ring0:
    # Now that everything's set up, we can safely call memmove to move the
    # regs32 struct to the desired location.
    push 0x44
    push esi
    push edi
    call memmove

    # Finally, we switch to using the new stack.
    mov esp, edi

.Lreturn:
    # Pop registers back off of the stack
    pop gs
    pop fs
    pop es
    pop ds
    popa

    # Skip the interrupt number and error code that were pushed onto the stack
    add esp, 8

    # Return from the interrupt handler
    iret
.size interrupt_common, .-interrupt_common

# These are lists of pointers to the handler stub functions that idt.cpp will
# use to initialize the IDT entries.
.macro function_pointers prefix, from, to
.int prefix&from
.if to-from
function_pointers prefix, %(from+1), to
.endif
.endm

.section .data
.globl _isr_handlers
.type _isr_handlers, @object
.hidden _isr_handlers
_isr_handlers:
function_pointers isr_, 0, (IDT_NUM_ISRS - 1)
.size _isr_handlers, .-_isr_handlers

.globl _irq_handlers
.type _irq_handlers, @object
.hidden _irq_handlers
_irq_handlers:
function_pointers irq_, 0, (IDT_NUM_IRQS - 1)
.size _irq_handlers, .-_irq_handlers

.globl _ext_handlers
.type _ext_handlers, @object
.hidden _ext_handlers
_ext_handlers:
function_pointers ext_, 0, (IDT_NUM_EXT - 1)
.size _ext_handlers, .-_ext_handlers

# Below this point are the handler stub functions that push the interrupt number
# and (if necessary) a default error code of 0 onto the stack and then call into
# interrupt_common.
.section .text

.macro isr_without_error_code n
.type isr_&n&, @function
isr_&n&:
    push 0
    push n
    jmp interrupt_common
.size isr_&n&, .-isr_&n&
.endm

.macro isr_with_error_code n
.type isr_&n&, @function
isr_&n&:
    push n
    jmp interrupt_common
.size isr_&n&, .-isr_&n&
.endm

.macro isrs_without_error_code from, to
isr_without_error_code from
.if to-from
isrs_without_error_code %(from+1), to
.endif
.endm

.macro isrs_with_error_code from, to
isr_with_error_code from
.if to-from
isrs_with_error_code %(from+1), to
.endif
.endm

isrs_without_error_code 0, 7
isr_with_error_code 8
isr_without_error_code 9
isrs_with_error_code 10, 14
isrs_without_error_code 15, 16
isr_with_error_code 17
isrs_without_error_code 18, 29
isr_with_error_code 30
isr_without_error_code 31

.macro irq n
.type irq_&n&, @function
irq_&n&:
    push 0
    push (n + IDT_IRQS_START)
    jmp interrupt_common
.size irq_&n&, .-irq_&n&
.endm

.macro irqs from, to
irq from
.if to-from
irqs %(from+1), to
.endif
.endm

irqs 0, (IDT_NUM_IRQS - 1)

.macro ext_handler n
.type ext_&n&, @function
ext_&n&:
    push 0
    push (n + IDT_EXT_START)
    jmp interrupt_common
.size ext_&n&, .-ext_&n&
.endm

.macro ext_handlers from, to
ext_handler from
.if to-from
ext_handlers %(from+1), to
.endif
.endm

ext_handlers 0, (IDT_NUM_EXT - 1)

.att_syntax
