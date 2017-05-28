.intel_syntax noprefix

.globl setjmp
.globl longjmp

.section .text

setjmp:
    push ebp
    mov ebp, esp

    # Find the address of the setjmp structure
    mov ecx, [ebp + 8]

    # Save the return address
    mov edx, [ebp + 4]
    mov [ecx], edx

    # Save the old ESP value
    mov edx, ebp
    add edx, 4
    mov [ecx + 4], edx

    # Save the old EBP value
    mov edx, [ebp]
    mov [ecx + 8], edx

    # Save other miscellaneous registers
    mov [ecx + 12], ebx
    mov [ecx + 16], edi
    mov [ecx + 20], esi

    # Return 0
    mov eax, 0
    mov esp, ebp
    pop ebp
    ret

longjmp:
    # Don't bother with the prologue: where we're going, we don't need prologues

    # Find the address of the setjmp structure
    mov ecx, [esp + 4]

    # Find the value we're supposed to return
    mov eax, [esp + 8]

    # Make sure the value isn't zero
    test eax, eax
    jnz .Lgood_val

    mov eax, 1

.Lgood_val:

    # Restore the stack positioning (ESP and EBP)
    mov esp, [ecx + 4]
    mov ebp, [ecx + 8]

    # Put the return address onto the stack so we can return to it
    mov edx, [ecx]
    mov [esp], edx

    # Restore miscellaneous registers
    mov ebx, [ecx + 12]
    mov edi, [ecx + 16]
    mov esi, [ecx + 20]

    # Finally, magically return back to the original location
    ret
