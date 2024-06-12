bits 16

section _TEXT class=code

global _bios_putc
_bios_putc:
    ; Make new stack frame
    push bp
    mov bp, sp

    push bx

    ; [bp + 0] - old call frame
    ; [bp + 2] - return address (small memory model => 2 bytes)
    ; [bp + 4] - first argument - character to be displayed
    ; [bp + 6] - second argument - page

    mov ah, 0Eh         ; AH = BIOS sub-function
    mov al, [bp + 4]    ; AL = character to be displayed
    mov bh, [bp + 6]    ; BH = page
    int 10h

    pop bx

    ; Restore old stack frame
    mov sp, bp
    pop bp
    ret
