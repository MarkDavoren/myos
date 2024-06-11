;
; Print a 16 bit number to the screen as four hex digits
; Parameters
;   - ax: 16 bit number to be printed as 4 hex chars
;
putnum:
    push di
    push dx
    push cx
    push bx
    push ax

    mov dx,ax
    mov cx,4
.loop
    and ax,0xF000
    shr ax,12
    mov bx,.nums
    mov di,ax
    mov al,[bx + di]

    mov ah, 0x0E        ; Set function to teletype mode
    mov bh, 0           ; Set page number to 0
    int 0x10            ; Call bios interrupt to display AL

    shl dx,4
    mov ax,dx

    dec cx
    jnz .loop

    pop ax
    pop bx
    pop cx
    pop dx
    pop di

    ret

.nums   DB '0123456789ABCDEF'
