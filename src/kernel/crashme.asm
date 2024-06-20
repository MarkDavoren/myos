[bits 32]

global crashMeDiv0
crashMeDiv0:
    ; div by 0
    mov ecx, 0x1337
    mov eax, 0
    div eax

    cli
    hlt

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

global crashMeInt8
crashMeInt8:
    mov eax, 0x10001
    mov ebx, 0x20002
    mov ecx, 0x30003
    mov edx, 0x40004
    mov esi, 0x50005
    mov edi, 0x60006
    int 08h

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
