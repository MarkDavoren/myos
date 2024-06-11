org 0x0
bits 16

%define ENDL 0x0D, 0x0A

start:
    mov si, hello_msg
    call puts

    cli
    hlt


;
; Prints a string to the screen
; Params:
;   - ds:si points to string
;
puts:
    ; save registers we will modify
    push si
    push ax
    push bx

.loop:
    lodsb               ; Loads next character from DS:SI into al and increments SI
    or al, al           ; Does nothing, but sets the Z flag
    jz .done            ; If char was null then jump

    mov ah, 0x0E        ; Set function to teletype mode
    mov bh, 0           ; Set page number to 0
    int 0x10            ; Call bios interrupt

    jmp .loop

.done:
    pop bx
    pop ax
    pop si    
    ret

hello_msg: db 'Hello world from the kernel!!', ENDL, 0

times 510-($-$$) db 0
dw 0xAA55
