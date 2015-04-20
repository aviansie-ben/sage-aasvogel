.intel_syntax noprefix

.globl gdt_flush
.type gdt_flush, @function
gdt_flush:
    # We are passed a pointer to a structure that the CPU uses to find and load
    # the GDT.
    mov eax, [esp + 0x4]
    
    # Request that the CPU load the GDT pointer we have received
    lgdt [eax]
    
    # Update the segment registers to make sure the CPU isn't caching any of
    # them.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    # The only way to update CS is with a far jump, so we do that here.
    jmp 0x08 : .Lend

.Lend:
    ret
.size gdt_flush, .-gdt_flush

.globl tss_load
.type tss_load, @function
tss_load:
    # We are passed the offset of the TSS to be loaded from the GDT. However, it
    # is in entries, so we multiply by the number of bytes per entry (8) to get
    # the byte offset.
    mov eax, [esp + 0x4]
    mov ecx, 8
    mul ecx
    
    # Request that the CPU load the TSS entry at the offset stored in AX.
    ltr ax
    
    ret
.size tss_load, .-tss_load

.att_syntax
