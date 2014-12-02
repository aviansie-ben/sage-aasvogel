[bits 32]
[global boot]

[extern _preinit_error]
[extern _preinit_setup_serial]
[extern _preinit_setup_paging]
[extern _preinit_page_dir]

[extern _ld_ctor_begin]
[extern _ld_ctor_end]

; Multiboot constants
MB_MODULEALIGN equ 1 << 0
MB_MEMINFO     equ 1 << 1
MB_VIDEOINFO   equ 1 << 2
MB_FLAGS       equ MB_MODULEALIGN | MB_MEMINFO
MB_MAGIC       equ 0x1BADB002
MB_CHECKSUM    equ -(MB_MAGIC + MB_FLAGS)

; Multiboot compliance header
[section .mb_header]
dd MB_MAGIC
dd MB_FLAGS
dd MB_CHECKSUM

[section .setup]

boot:
    ; Make sure that interrupts are turned off. We don't want any interrupts
    ; until the IDT is properly initialized.
    cli
    
    ; Initialize the stack pointer. Even printing a failure message requires a
    ; working stack... Note that since the stack is in the .bss section, we must
    ; subtract 0xC0000000 to get the physical address, since we haven't mapped
    ; to higher-half yet!
    mov esp, stack - 0xC0000000
    
    ; Check that we've been booted by a multiboot-compliant bootloader.
    cmp eax, 0x2BADB002
    jne not_multiboot
    
    ; Save a copy of the multiboot information structure pointer that's
    ; currently in EBX, as EBX will get clobbered during pre-initialization.
    push ebx
    
    ; Initialize basic serial bus support. We use this to write debug messages
    ; in case something goes very wrong during initialization.
    call _preinit_setup_serial
    
    ; Check that the CPU supports the CPUID instruction. CPUs that don't support
    ; CPUID will not allow modification to the ID flag (0x200000) in the EFLAGS
    ; register.
    
    ; Get the current value of EFLAGS into EAX and ECX
    pushfd
    pop eax
    mov ecx, eax
    
    ; Flip the ID bit in EAX and set EFLAGS to the value of EAX
    xor eax, 0x200000
    push eax
    popfd
    
    ; Get the resulting value of EFLAGS back into EAX
    pushfd
    pop eax
    
    ; Check that the ID bit has in fact changed in EFLAGS
    xor eax, ecx
    and eax, 0x200000
    jz not_cpuid
    
    ; Restore the original value of EFLAGS
    push ecx
    popfd
    
    ; If PAE is supported, we need to use it, otherwise we won't be able to
    ; initialize it later.
    
    ; Run CPUID with EAX=1 (CPUID_GETFEATURES)
    mov eax, 0x1
    cpuid
    
    ; Bit 0x40 in EDX is 1 if PAE is supported and 0 otherwise
    and edx, 0x40
    shr edx, 6
    push edx
    
setup_paging:
    ; Call C++ code to set up the paging directory so that the kernel gets
    ; mapped at 0xC0000000.
    call _preinit_setup_paging
    pop edx
    
    ; Move the address of the page directory set up by the above function into
    ; CR3.
    mov cr3, eax
    
    ; If PAE is supported, it should be enabled now
    cmp edx, 0
    je .enable
    
    mov ecx, cr4
    or ecx, 0x20
    mov cr4, ecx

.enable:
    ; Enable paging and enable the WP bit to ensure we're not changing read-only
    ; memory.
    mov eax, cr0
    or eax, 0x80010000
    mov cr0, eax
    
    ; The stack pointer should be modified to point to the higher-half mapped
    ; version of the stack so we don't get any unwelcome surprises when the
    ; kernel enables paging for real...
    add esp, 0xC0000000

run_static_ctors:
    ; Memory from _ld_ctor_begin to _ld_ctor_end contains a list of static class
    ; constructor functions that we should call before jumping into the kernel
    ; proper.
    mov ebx, _ld_ctor_begin
    jmp .test
    
.call_ctor:
    call [ebx]
    add ebx, 4

.test:
    cmp ebx, _ld_ctor_end
    jb .call_ctor
    
run_kernel:
    push success_error
    call _preinit_error

not_multiboot:
    push not_multiboot_error
    call _preinit_error
    jmp $

not_cpuid:
    push not_cpuid_error
    call _preinit_error
    jmp $

[section .setup_data]
not_multiboot_error db "FATAL: Must boot with a multiboot-compliant bootloader!", 0x00
not_cpuid_error db "FATAL: Processor does not support CPUID!", 0x00
success_error db "FATAL: Booted successfully!", 0x00
    
; Definition for the kernel-mode stack
[section .bss]
align 32
resb 0x4000 ; We allocate 16kB of stack space for the kernel
stack: ; This symbol points to the end of the stack
