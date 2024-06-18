bits 16

section _TEXT

;
; x86_outb(Uint8 port, Uint8 value)
;
; Output a byte value to a port
;
global x86_outb
x86_outb:
    [bits 32]
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

global x86_inb
x86_inb:
    [bits 32]
    mov dx, [esp + 4]
    xor eax, eax
    in al, dx
    ret