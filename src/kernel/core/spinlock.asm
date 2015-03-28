.intel_syntax noprefix

.globl spinlock_init
.globl spinlock_acquire
.globl spinlock_release

spinlock_init:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]
    mov dword ptr [eax], 0
    mov dword ptr [eax + 4], 0
    
    mov esp, ebp
    pop ebp
    ret

spinlock_acquire:
    push ebp
    mov ebp, esp
    pushfd # Store the old EFLAGS register for later. This is used to restore
           # the interrupt flag in spinlock_release.
    
    cli # A thread must not be interrupted while it is attempting to acquire or
        # when it has acquired a spinlock.
    
    mov eax, [ebp + 8]
    
.Lretry:
    lock bts dword ptr [eax], 0 # Attempt to acquire the lock.
    jnc .Lcleanup
    
    # Wait until the lock appears to be free, then retry.
.Lwait:
    pause # Tells the CPU that we're in a spinlock. Avoids problems when
          # hyperthreading is enabled.
    
    cmp dword ptr [eax], 0
    jne .Lwait
    
    jmp .Lretry
    
.Lcleanup:
    # Store the old value of the EFLAGS register in the spinlock to be restored
    # when spinlock_release is called.
    mov ecx, [ebp - 4]
    mov [eax + 4], ecx
    
    mov esp, ebp
    pop ebp
    ret

spinlock_release:
    push ebp
    mov ebp, esp
    sub esp, 4
    
    mov eax, [ebp + 8]
    
    # Read and reset the old EFLAGS value.
    mov ecx, [eax + 4]
    mov [ebp - 4], ecx
    mov dword ptr [eax + 4], 0
    
    # Free the lock.
    mov dword ptr [eax], 0
    
    # Restore the old EFLAGS value. This will re-enable interrupts if they were
    # disabled by spinlock_acquire.
    popfd
    
    mov esp, ebp
    pop ebp
    ret
