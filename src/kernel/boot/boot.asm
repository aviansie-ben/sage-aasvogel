.intel_syntax noprefix

.globl boot

# Multiboot compliance header
.section .mb_header
.int 0x1BADB002
.int 0b11
.int -(0x1BADB002 + 0b11)

.section .setup

boot:
    # Make sure that interrupts are turned off. We don't want any interrupts
    # until the IDT is properly initialized.
    cli
    
    # Initialize the stack pointer. Even printing a failure message requires a
    # working stack... Note that since the stack is in the .bss section, we must
    # subtract 0xC0000000 to get the physical address, since we haven't mapped
    # to higher-half yet!
    mov esp, offset (stack - 0xC0000000)
    
    # Check that we've been booted by a multiboot-compliant bootloader.
    cmp eax, 0x2BADB002
    jne not_multiboot
    
    # Some kernel arguments passed in via the boot loader need to be parsed
    # before executing any pre-initialization code.
    push ebx
    call _preinit_parse_cmdline
    pop ebx
    
    # EBX contains a pointer to a multiboot information structure. Correct the
    # address to point to higher half memory and put it on the stack for later,
    # when we call the kernel_main function.
    add ebx, 0xC0000000
    push ebx
    
    # Initialize basic serial bus support. We use this to write debug messages
    # in case something goes very wrong during initialization.
    call _preinit_setup_serial
    
    # Check that the CPU supports the CPUID instruction. CPUs that don't support
    # CPUID will not allow modification to the ID flag (0x200000) in the EFLAGS
    # register.
    
    # Get the current value of EFLAGS into EAX and ECX
    pushfd
    pop eax
    mov ecx, eax
    
    # Flip the ID bit in EAX and set EFLAGS to the value of EAX
    xor eax, 0x200000
    push eax
    popfd
    
    # Get the resulting value of EFLAGS back into EAX
    pushfd
    pop eax
    
    # Check that the ID bit has in fact changed in EFLAGS
    xor eax, ecx
    and eax, 0x200000
    jz not_cpuid
    
    # Restore the original value of EFLAGS
    push ecx
    popfd
    
    # If PAE is supported, we need to use it, otherwise we won't be able to
    # initialize it later.
    
    # Run CPUID with EAX=1 (CPUID_GETFEATURES)
    mov eax, 0x1
    cpuid
    
    # Bit 0x1 in EDX is 1 if an FPU was detected and 0 otherwise
    test edx, 0x1
    jz no_fpu
    
    # Bit 0x40 in EDX is 1 if PAE is supported and 0 otherwise
    and edx, 0x40
    shr edx, 6
    push edx

fpu_init:
    push offset serial_dbg_init_fpu
    call _preinit_write_serial
    add esp, 4
    
    # We must first check that the processor detected that the FPU is 80387-
    # compatible. We do not support the 80287. To do this, we check that the
    # ET bit in CR0 is set to 1.
    mov eax, cr0
    mov ebx, eax
    and ebx, 0x10
    jz no_fpu
    
    # Now that we know that an FPU is present, we set up CR0 to allow it to be
    # used. To do this, we set the MP bit to 1, the EM bit to 0, and the NE bit
    # to 1.
    and eax, ~0x4
    or eax, 0x22
    mov cr0, eax
    
    # Now we actually request the FPU to initialize itself.
    fninit
    
setup_paging:
    # Call C++ code to set up the paging directory so that the kernel gets
    # mapped at 0xC0000000.
    call _preinit_setup_paging
    pop edx
    
    # Move the address of the page directory set up by the above function into
    # CR3.
    mov cr3, eax
    
    # If PAE is supported and was not explicitly disabled, it should be enabled
    # now.
    cmp edx, 0
    je .enable
    
    mov eax, [_preinit_pae_enabled]
    test eax, eax
    jz .enable
    
    mov ecx, cr4
    or ecx, 0x20
    mov cr4, ecx

.enable:
    push offset serial_dbg_enable_paging
    call _preinit_write_serial
    add esp, 4
    
    # Enable paging and enable the WP bit to ensure we're not changing read-only
    # memory.
    mov eax, cr0
    or eax, 0x80010000
    mov cr0, eax
    
    # The stack pointer should be modified to point to the higher-half mapped
    # version of the stack so we don't get any unwelcome surprises when the
    # kernel enables paging for real...
    add esp, 0xC0000000

run_static_ctors:
    push offset serial_dbg_static_ctors
    call _preinit_write_serial
    add esp, 4
    
    call _init
    
run_kernel:
    push offset serial_dbg_kernel_main
    call _preinit_write_serial
    add esp, 4
    
    # At this point, pre-initialization is complete and the environment is ready
    # to be able to run standard C++ code. The kernel_main function will handle
    # the rest...
    call kernel_main
    
    # kernel_main is not designed to return, so this code should never be
    # reached, but it is included just in case.
    push offset kernel_return_error
    call _preinit_error
    jmp $

not_multiboot:
    push offset not_multiboot_error
    call _preinit_error
    jmp $

not_cpuid:
    push offset not_cpuid_error
    call _preinit_error
    jmp $

no_fpu:
    push offset no_fpu_error
    call _preinit_error
    jmp $

.section .setup_data
not_multiboot_error: .ascii "FATAL: Must boot with a multiboot-compliant bootloader!\0"
not_cpuid_error: .ascii "FATAL: Processor does not support CPUID!\0"
no_fpu_error: .ascii "FATAL: Processor does not have a 80387-compatible FPU!\0"
kernel_return_error: .ascii "FATAL: kernel_main has returned unexpectedly!\0"

serial_dbg_init_fpu: .ascii "Initializing FPU...\r\n\0"
serial_dbg_enable_paging: .ascii "Enabling pre-initialization paging...\r\n\0"
serial_dbg_static_ctors: .ascii "Running C++ static constructors...\r\n\0"
serial_dbg_kernel_main: .ascii "Calling into kernel_main...\r\n\0"

# Definition for the kernel-mode stack
.section .bss
.align 32
.skip 0x4000 # We allocate 16kB of stack space for the kernel
stack:

.att_syntax
