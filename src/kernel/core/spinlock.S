.intel_syntax noprefix

.globl spinlock_init
.type spinlock_init, @function
spinlock_init:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]
    mov dword ptr [eax], 0
    mov dword ptr [eax + 4], 0
    
    mov esp, ebp
    pop ebp
    ret
.size spinlock_init, .-spinlock_init

.globl spinlock_acquire
.type spinlock_acquire, @function
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
    # when spinlock_release is called. The carry flag will not be stored, and
    # will always be replaced with a 1, since that bit of the spinlock
    # information is used for testing a lock.
    mov ecx, [ebp - 4]
    or ecx, 1
    mov [eax], ecx
    
    mov esp, ebp
    pop ebp
    ret
.size spinlock_acquire, .-spinlock_acquire

.globl spinlock_release
.type spinlock_release, @function
spinlock_release:
    push ebp
    mov ebp, esp
    sub esp, 4
    
    mov eax, [ebp + 8]
    
    # Read the old EFLAGS value.
    mov ecx, [eax]
    mov [ebp - 4], ecx
    
    # Free the lock.
    mov dword ptr [eax], 0
    
    # Restore the old EFLAGS value. This will re-enable interrupts if they were
    # disabled by spinlock_acquire.
    popfd
    
    mov esp, ebp
    pop ebp
    ret
.size spinlock_release, .-spinlock_release
