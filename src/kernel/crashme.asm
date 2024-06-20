[bits 32]

global crashMeDiv0
crashMeDiv0:
    ; div by 0
    mov ecx, 0x1337
    mov eax, 0
    div eax

    cli
    hlt

;
; Note: Using int instructions to simulate "one of the architecturally-defined exceptions, the processor
; generates an interrupt to the correct vector (to access the exception handler) but 
; **does not push an error code on the stack**"
; So if we generate an exception for an exception that normally pushes an error code, it won't happen.
; Everything will work until the ISR prepares to return. Unfortunately, instead of popping off the error code
; it will pop off the EIP! and iret will jmp to the CS instead. VERY BAD!!
;
global crashMeInt1
crashMeInt1:
    mov eax, 0x10001
    mov ebx, 0x20002
    mov ecx, 0x30003
    mov edx, 0x40004
    mov esi, 0x50005
    mov edi, 0x60006
    int 01h

    cli
    hlt

global crashMeInt6
crashMeInt6:
    mov eax, 0x10001
    mov ebx, 0x20002
    mov ecx, 0x30003
    mov edx, 0x40004
    mov esi, 0x50005
    mov edi, 0x60006
    int 06h

    cli
    hlt

global crashMeInt64h
crashMeInt64h:
    mov eax, 0x10001
    mov ebx, 0x20002
    mov ecx, 0x30003
    mov edx, 0x40004
    mov esi, 0x50005
    mov edi, 0x60006
    int 100

    cli
    hlt
