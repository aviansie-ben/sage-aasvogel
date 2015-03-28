.intel_syntax noprefix

.globl _isr_handlers
.globl _irq_handlers
.globl idt_flush

.section .text
idt_flush:
    # Get the pointer that we have been passed to the structure that points to
    # the IDT
    mov eax, [esp + 0x4]
    
    # Request that the CPU load the IDT pointer we have received
    lidt [eax]
    
    ret

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
    je .do_call
    
    sub dword ptr [esp], 8
    sub esp, 8
    sub ebp, 8
    mov eax, esp
    mov ebx, eax
    add ebx, 0x4C
    
.alloc_ring0:
    mov ecx, dword ptr [eax + 0x8]
    mov dword ptr [eax], ecx
    add eax, 4
    
    cmp eax, ebx
    jl .alloc_ring0
    
    mov dword ptr [ebx], 0
    mov dword ptr [ebx + 0x4], 0
    
.do_call:
    # Call the C interrupt handler
    mov edx, [esp]
    push edx
    call _idt_handle
    add esp, 4
    
    # Now we need to check whether the code we're returning to is ring 0 code.
    # If it is, then useresp and ss won't get popped by IRET. So, we need to
    # move the structure again...
    
    mov edx, [esp]
    push edx
    call _idt_is_ring0
    add esp, 4
    cmp eax, 0
    je .return
    
    mov eax, esp
    mov ebx, eax
    add ebx, 0x50
    add dword ptr [esp], 8
    add esp, 8
    add ebp, 8

.free_ring0:
    mov ecx, dword ptr [ebx - 0x8]
    mov dword ptr [ebx], ecx
    sub ebx, 4
    
    cmp ebx, eax
    jge .free_ring0

.return:
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

# These are lists of pointers to the handler stub functions that idt.cpp will
# use to initialize the IDT entries.
.section .data
_isr_handlers:
    .int isr_0
    .int isr_1
    .int isr_2
    .int isr_3
    .int isr_4
    .int isr_5
    .int isr_6
    .int isr_7
    .int isr_8
    .int isr_9
    .int isr_10
    .int isr_11
    .int isr_12
    .int isr_13
    .int isr_14
    .int isr_15
    .int isr_16
    .int isr_17
    .int isr_18
    .int isr_19
    .int isr_20
    .int isr_21
    .int isr_22
    .int isr_23
    .int isr_24
    .int isr_25
    .int isr_26
    .int isr_27
    .int isr_28
    .int isr_29
    .int isr_30
    .int isr_31

_irq_handlers:
    .int irq_0
    .int irq_1
    .int irq_2
    .int irq_3
    .int irq_4
    .int irq_5
    .int irq_6
    .int irq_7
    .int irq_8
    .int irq_9
    .int irq_10
    .int irq_11
    .int irq_12
    .int irq_13
    .int irq_14
    .int irq_15

# Below this point are the handler stub functions that push the interrupt number
# and (if necessary) a default error code of 0 onto the stack and then call into
# interrupt_common.
.section .text
isr_0:
    push 0
    push 0
    jmp interrupt_common

isr_1:
    push 0
    push 1
    jmp interrupt_common

isr_2:
    push 0
    push 2
    jmp interrupt_common

isr_3:
    push 0
    push 3
    jmp interrupt_common

isr_4:
    push 0
    push 4
    jmp interrupt_common

isr_5:
    push 0
    push 5
    jmp interrupt_common
    
isr_6:
    push 0
    push 6
    jmp interrupt_common

isr_7:
    push 0
    push 7
    jmp interrupt_common

isr_8:
    push 8
    jmp interrupt_common

isr_9:
    push 0
    push 9
    jmp interrupt_common

isr_10:
    push 10
    jmp interrupt_common

isr_11:
    push 11
    jmp interrupt_common

isr_12:
    push 12
    jmp interrupt_common

isr_13:
    push 13
    jmp interrupt_common

isr_14:
    push 14
    jmp interrupt_common

isr_15:
    push 0
    push 15
    jmp interrupt_common

isr_16:
    push 0
    push 16
    jmp interrupt_common

isr_17:
    push 0
    push 17
    jmp interrupt_common

isr_18:
    push 0
    push 18
    jmp interrupt_common

isr_19:
    push 0
    push 19
    jmp interrupt_common

isr_20:
    push 0
    push 20
    jmp interrupt_common

isr_21:
    push 0
    push 21
    jmp interrupt_common

isr_22:
    push 0
    push 22
    jmp interrupt_common

isr_23:
    push 0
    push 23
    jmp interrupt_common

isr_24:
    push 0
    push 24
    jmp interrupt_common

isr_25:
    push 0
    push 25
    jmp interrupt_common

isr_26:
    push 0
    push 26
    jmp interrupt_common

isr_27:
    push 0
    push 27
    jmp interrupt_common

isr_28:
    push 0
    push 28
    jmp interrupt_common

isr_29:
    push 0
    push 29
    jmp interrupt_common

isr_30:
    push 0
    push 30
    jmp interrupt_common

isr_31:
    push 0
    push 31
    jmp interrupt_common

irq_0:
    push 0
    push 32
    jmp interrupt_common

irq_1:
    push 0
    push 33
    jmp interrupt_common

irq_2:
    push 0
    push 34
    jmp interrupt_common

irq_3:
    push 0
    push 35
    jmp interrupt_common

irq_4:
    push 0
    push 36
    jmp interrupt_common

irq_5:
    push 0
    push 37
    jmp interrupt_common

irq_6:
    push 0
    push 38
    jmp interrupt_common

irq_7:
    push 0
    push 39
    jmp interrupt_common

irq_8:
    push 0
    push 40
    jmp interrupt_common

irq_9:
    push 0
    push 41
    jmp interrupt_common

irq_10:
    push 0
    push 42
    jmp interrupt_common

irq_11:
    push 0
    push 43
    jmp interrupt_common

irq_12:
    push 0
    push 44
    jmp interrupt_common

irq_13:
    push 0
    push 45
    jmp interrupt_common

irq_14:
    push 0
    push 46
    jmp interrupt_common

irq_15:
    push 0
    push 47
    jmp interrupt_common

.att_syntax
