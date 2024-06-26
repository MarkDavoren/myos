bits 16

section _TEXT

;
; xi686_outb(Uint8 port, Uint8 value)
;
; Output a byte value to a port
;
global i686_outb
i686_outb:
    [bits 32]
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

;
; Uint8 xi686_inb(Uint8 port)
;
; Input a byte value from a port
;
global i686_inb
i686_inb:
    [bits 32]
    mov dx, [esp + 4]
    xor eax, eax
    in al, dx
    ret

;
; Just leave me alone and let me stop!
;

global i686_halt
i686_halt:
    cli
    hlt

;
; Disable interrupts
;

global i686_disableInterrupts
i686_disableInterrupts:
    cli
    ret

;
; Enable interrupts
;
global i686_enableInterrupts
i686_enableInterrupts:
    sti
    ret
