bits 16

section .entry

extern start
extern __bss_start
extern __end

;
; Documentation on switching into protected mode and back to real mode is found at
;   file:///C:/Users/m/Downloads/325462-sdm-vol-1-2abcd-3abcd-4.pdf
;   Section 10.9.1 "Switching to Protected Mode"
;   Section 10.9.2 "Switching back to Real-Address Mode"
; and also
;   https://wiki.osdev.org/Protected_Mode
;
; To switch to protected mode
; 1. Disable interrupts
;       Strictly speaking we should also disable NMIs (Non-Maskable Interrupts) but they would occur
;       only if there is a hardware failure or the OS has setup a time which we haven't done
; 2. Enable A20 Gate
;       Archaic and sordid hack for backwards compatibility from the 1980s!
; 3. Load GDTR (Global Descriptor Table)
; 4. Set PE (Protection Enable) flag in CR0
; 5. Execute a far JMP or CALL instruction
; -. Optional. Setup LDT (Local Descriptor Table)
; -. Task register setup ???
; 6. Set segment registers. CS was set by JMP or CALL
; -. IDT setup???
; 7. Enable interrupts

global entry
entry:
    [bits 16]
    cli                 ; Disable interrupts while we setup the stack and go into protected mode

    mov [bootDrive], dl ; Save param
    mov [partitionTableOffset], si
    mov [partitionTableSegment], di


    ; Setup stack
    mov ax, ds          ; stage 1 set DS to the STAGE2_LOAD_SEGMENT
    mov ss, ax
    mov sp, 0xFFF0      ; Stack will go down from the top of the load segment
    mov bp, sp

    call checkBiosDiskExtensions

    call enableA20      ; 2 - Enable A20 gate

    lgdt [GDTDesc]      ; 3 - Load GDT

    ; 4 - Set protection enable flag in CR0
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; 5 - Far JMP
    jmp dword 08h:.pmode

.pmode:
    ; We are now in protected mode!!
    [bits 32]

    ; 6 - Set segment registers
    mov ax, 0x10        ; 32 bit data segment in GDT
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Prepare for C land
    ; Clear bss
    mov edi, __bss_start
    mov ecx, __end
    sub ecx, edi        ; ECX = __end - __bss_start
    mov al, 0           ; Load zero into AL
    cld                 ; Count down from ECX to zero
    rep stosb           ; Store AL into ES:EDI, EDI++, ECX-- until ECX == 0

    ; Load param to start(Uint16 bootDrive, Partition* partitionTable)
    mov dx, [partitionTableSegment]
    shl edx, 4
    xor eax, eax
    mov ax, [partitionTableOffset]
    add edx, eax
    push edx

    xor edx, edx
    mov dl, [bootDrive]
    push edx

    ; Off to C land
    call start

    ; Halt when start() returns
    cli
    hlt

checkBiosDiskExtensions:
    [bits 16]

    mov ah, 0x41        ; BIOS function
    mov dl, [bootDrive] ; Drive
    mov bx, 0x55AA      ; Magic number
    stc
    int 13h             ; Clear CF => has extensions
    mov eax, 1
    sbb eax, 0          ; Subtract with borrow. If CF is clear then EAX = 1, otherwise EAX = 0
    mov [biosHasDiskExtension], eax

    ret

global biosHasDiskExtension
biosHasDiskExtension: dq 0

enableA20:
    [bits 16]
    ; disable keyboard
    call A20WaitInput
    mov al, KbdControllerDisableKeyboard
    out KbdControllerCommandPort, al

    ; read control output port
    call A20WaitInput
    mov al, KbdControllerReadCtrlOutputPort
    out KbdControllerCommandPort, al

    call A20WaitOutput
    in al, KbdControllerDataPort
    push eax

    ; write control output port
    call A20WaitInput
    mov al, KbdControllerWriteCtrlOutputPort
    out KbdControllerCommandPort, al
    
    call A20WaitInput
    pop eax
    or al, 2                                    ; bit 2 = A20 bit
    out KbdControllerDataPort, al

    ; enable keyboard
    call A20WaitInput
    mov al, KbdControllerEnableKeyboard
    out KbdControllerCommandPort, al

    call A20WaitInput

    ret

A20WaitInput:
    [bits 16]
    ; wait until status bit 2 (input buffer) is 0
    ; by reading from command port, we read status byte
    in al, KbdControllerCommandPort
    test al, 2
    jnz A20WaitInput
    ret

A20WaitOutput:
    [bits 16]
    ; wait until status bit 1 (output buffer) is 1 so it can be read
    in al, KbdControllerCommandPort
    test al, 1
    jz A20WaitOutput
    ret

KbdControllerDataPort               equ 0x60
KbdControllerCommandPort            equ 0x64
KbdControllerDisableKeyboard        equ 0xAD
KbdControllerEnableKeyboard         equ 0xAE
KbdControllerReadCtrlOutputPort     equ 0xD0
KbdControllerWriteCtrlOutputPort    equ 0xD1

; Global Descriptor Table
;
;   | Base   | Flags | Limit | Access | Base  |
;   |31    24|3     0|19   16|7      0|23   16|
;
;   | Base                   | Limit          |
;   |15                     0|15             0|
;
; Base: a 32 bit value (spread over three sections)
;
; Limit: a 20 bit value (spread over two sections)
;
; Access
;   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
;   | P |  DPL  | S | E | DC| RW| A |
;
; P: Present. The segment entry is valid
; DPL: Descriptor Privilege Level. 0 = highest (kernel). 3 = lowest (user)
; S: Descriptor. 0 = system segment. 1 = code or data segment
; E: Executable. 0 = data segment. 1 = code segment
; DC: Direction/Conforming
;       For data segments - direction. 0 = segement grows up. 1 = down
;       For code segments - conforming.
;           0 = code in this segment can only be executed from the ring set in DPL
;           1 = code in this segment can be executed from an equal or lower privilege, but not higher
; RW: Read/Write
;       For code segments. 0 = not-readable. 1 = readable. (Neither allows writes)
;       For data segment. 0 = not writeable. 1 = writeable (Reads are always allowed)
; A: Access. CPU will set this bit when the segment is access unless set to 1 already
;
; Flags
;   | 3 | 2 | 1 | 0 |
;   | G | DB| L |   |
; G: Granularity. 0 = Limit is in 1 byte blocks. 1 = Limit is in 4K blocks
; DB: Size. 0 = 16 bit protected mode. 1 = 32 bit protected mode
; L: Long mode. 1 = descriptor defines a 64 bit code segment - DB should always be 0. 0 otherwise
; Bit 0 is reserved
; 

GDT:
    ; 0x00 - NULL descriptor
    dq 0            ; First descriptor should always be a null descriptor

    ; 0x08 - 32 bit code segment
    dw 0xFFFF       ; Limit[0..15]
    dw 0            ; Base[0..15]
    db 0            ; Base[16..23]
    db 10011010b    ; Present, code/data, executable, readable
    db 11001111b    ; Limit in 4K blocks, 32 bit protected, Limit[16..19]
    db 0            ; Base[24..31]

    ; 0x10 - 32 bit data segment
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b    ; Present, code/data, data, readable
    db 11001111b    ; Limit in 4K blocks, 32 bit protected, Limit[16..19]
    db 0

    ; 0x18 - 16 bit code segment
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b    ; Present, code/data, executable, readable
    db 00001111b    ; Limit in bytes, 16 bit protected, Limit[16..19]
    db 0

    ; 0x20 - 16 bit data segment
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b    ; Present, code/data, data, readable
    db 00001111b    ; Limit in bytes, 16 bit protected, Limit[16..19]
    db 0

GDTDesc:                    ; Global Descriptor Table Descriptor
    dw GDTDesc - GDT - 1    ; Size of the GDT in bytes - 1 (0 means 8192 entries not zero)
    dd GDT                  ; Address of GDT
   
bootDrive: db 0
partitionTableSegment: dw 0
partitionTableOffset: dw 0