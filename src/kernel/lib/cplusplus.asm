.intel_syntax noprefix

.globl _init
.type _init, @function

.section .init-head
_init:
push ebp
mov ebp, esp

# The linker will insert the .init section (which is in crtbegin.o and crtend.o)
# in between .init-head and .init-foot, so it will be executed here.

.section .init-foot
pop ebp
ret

.att_syntax
