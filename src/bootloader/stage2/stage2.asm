bits 16

section _ENTRY class=code

extern _cstart_

global entry

entry:
    ; Setup stack
    cli                 ; Disable interrupts while mcuking around with the stack pointer
    mov ax, ds          ; stage 1 set DS to the STAGE2_LOAD_SEGMENT
    mov ss, ax
    mov sp, 0           ; Stack will go down from the top of the load segment
    mov bp, sp
    sti

    ; Expecting boot drive in DL
    ; call cstart(boot_drive)
    xor dh, dh
    push dx
    call _cstart_

    cli
    hlt
