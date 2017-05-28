.intel_syntax noprefix

.globl eflags_save
.type eflags_save, @function
eflags_save:
    push ebp
    mov ebp, esp

    pushfd
    mov eax, [ebp - 4]

    mov esp, ebp
    pop ebp
    ret
.size eflags_save, .-eflags_save

.globl eflags_load
.type eflags_load, @function
eflags_load:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]
    push eax
    popfd

    mov esp, ebp
    pop ebp
    ret
.size eflags_load, .-eflags_load

.globl spinlock_init
.type spinlock_init, @function
spinlock_init:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]
    mov dword ptr [eax], 0

    mov esp, ebp
    pop ebp
    ret
.size spinlock_init, .-spinlock_init

.globl _spinlock_acquire
.type _spinlock_acquire, @function
_spinlock_acquire:
    push ebp
    mov ebp, esp

    cli # A thread must not be interrupted while it is attempting to acquire or
        # when it has acquired a spinlock.

    mov eax, [ebp + 8]

.La_retry:
    lock bts dword ptr [eax], 0 # Attempt to acquire the lock.
    jnc .La_cleanup

    # Wait until the lock appears to be free, then retry.
.La_wait:
    pause # Tells the CPU that we're in a spinlock. Avoids problems when
          # hyperthreading is enabled.

    cmp dword ptr [eax], 0
    jne .La_wait

    jmp .La_retry

.La_cleanup:
    # Store the old value of the EFLAGS register in the spinlock to be restored
    # when spinlock_release is called. The carry flag will not be stored, and
    # will always be replaced with a 1, since that bit of the spinlock
    # information is used for testing a lock.
    mov ecx, [ebp + 12]
    or ecx, 1
    mov [eax], ecx

    mov esp, ebp
    pop ebp
    ret
.size _spinlock_acquire, .-_spinlock_acquire

.globl _spinlock_try_acquire
.type _spinlock_try_acquire, @function
_spinlock_try_acquire:
    push ebp
    mov ebp, esp

    cli # A thread must not be interrupted while it is attempting to acquire or
        # when it has acquired a spinlock.

    mov eax, [ebp + 8]

    lock bts dword ptr [eax], 0 # Attempt to acquire the lock.
    jnc .Lta_cleanup

    # We have failed to acquire the lock, so give up.
    push [ebp + 12]
    popfd

    mov esp, ebp
    pop ebp
    mov eax, 0
    ret

.Lta_cleanup:
    # Store the old value of the EFLAGS register in the spinlock to be restored
    # when spinlock_release is called. The carry flag will not be stored, and
    # will always be replaced with a 1, since that bit of the spinlock
    # information is used for testing a lock.
    mov ecx, [ebp + 12]
    or ecx, 1
    mov [eax], ecx

    mov esp, ebp
    pop ebp
    mov eax, 1
    ret
.size _spinlock_try_acquire, .-_spinlock_try_acquire

.globl _spinlock_release
.type _spinlock_release, @function
_spinlock_release:
    push ebp
    mov ebp, esp

    mov ecx, [ebp + 8]

    # Read the old EFLAGS value.
    mov eax, [ecx]

    # Free the lock.
    mov dword ptr [ecx], 0

    mov esp, ebp
    pop ebp
    ret
.size _spinlock_release, .-_spinlock_release
