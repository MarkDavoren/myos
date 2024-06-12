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

;
; void _cdecl x86_div64_32(uint64_t dividend, uint32_t divisor, uint64_t* quotient, uint32_t* remainder);
;
; When compiling C files in 16 bit mode, wcc will use functions from a system library to handle
; division and mod of 32 bit or higher operands. Since we don't have access to those libraries,
; we get a symbol, e.g., U8DR or U8DQ, not found error. So implement our own divide and mod code
;
; 2 remainder   <- bp + 18
; 2 quotient    <- bp + 16
; 4 divisor     <- bp + 12
; 8 dividend    <- bp + 4
; 2 return offset
; 2 old bp      <- bp
;

global _x86_div64_32
_x86_div64_32:

    ; make new call frame
    push bp             ; save old call frame
    mov bp, sp          ; initialize new call frame

    push bx

    ; divide upper 32 bits
    mov eax, [bp + 8]   ; eax <- upper 32 bits of dividend
    mov ecx, [bp + 12]  ; ecx <- divisor
    xor edx, edx
    div ecx             ; eax - quot, edx - remainder

    ; store upper 32 bits of quotient
    mov bx, [bp + 16]
    mov [bx + 4], eax

    ; divide lower 32 bits
    mov eax, [bp + 4]   ; eax <- lower 32 bits of dividend
                        ; edx <- old remainder
    div ecx

    ; store results
    mov [bx], eax
    mov bx, [bp + 18]
    mov [bx], edx

    pop bx

    ; restore old call frame
    mov sp, bp
    pop bp
    ret
