bits 16

section _TEXT

; Create macros to enter 16 bit real mode and 32 bit protected mode.
; Macros are a lot easier than functions since, e.g., with a function to go from 32PM to 16RM
; the caller would push a 32 bit return address, but the callee return would pop a 16 bit address

;
; Enter 16 bit real mode
; 1. JMP to 16 bit protected mode
; 2. Clear protected mode flag in CR0
; 3. JMP to a 16 bit real mode code segment
; 4. Set segment registers to point to 16 bit real mode segment
; 5. Enable interrupts
;

%macro x86_enterRealMode 0
    [bits 32]
    jmp word 18h:.pmode16   ; 18h refers to the 4th GDT entry which is a 16 bit code segment
.pmode16:
    [bits 16]

    mov eax, cr0
    and al, ~1              ; clear Protected mode flag
    mov cr0, eax

    jmp word 00h:.rmode     ; 00h refers to first GDT entry which is a 16 bit real mode segment
.rmode:

    mov ax, 00h             ; 00h refers to first GDT entry which is a 16 bit real mode segment
    mov ds, ax
    mov ss, ax

    sti
%endmacro

;
; Enter 32 bit protected mode
; 1. Disable interrupts
; 2. Set protected mode flag in CR0
; 3. JMP to a 32 bit protected mode code segment
; 4. Set segment registers to point to 32 bit protected data segment
;

%macro x86_enterProtectedMode 0
    [bits 16]
    cli

    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp dword 08h:.pmode    ; 08h refers to second GDT entry which is a 32 bit protected mode code segment
.pmode:
    [bits 32]

    mov ax, 0x10            ; 10h refers to third GDT entry which is a 32 bit protected mode data segment
    mov ds, ax
    mov ss, ax
%endmacro

;
; Convert a linear address to a segment:offset address
; 1. in  - 32 bit linear address
; 2. out - 16 bit segment
; 3. tmp - 32 bit register containing %4
; 4. out - 16 bit offset
;
; segment = linear >> 4
; offset  = linear & 0xF
;

%macro linearToSegmented 4
    mov %3, %1      ; linear address to eax
    shr %3, 4
    mov %2, %4
    mov %3, %1      ; linear address to eax
    and %3, 0xf
%endmacro

;################################
; Non-extended functions
;################################

;
; Bool _cdecl bios_getDriveParams(
;                   Uint8 driveNumber,
;                   Uint16* numCylinders,
;                   Uint16* numHeads,
;                   Uint16* numSectors)
;
; Gets the geometry of the specified drive
;
; Returns true/false on success/failure
;

global bios_getDriveParams
bios_getDriveParams:
    [bits 32]

    ; Make new stack frame
    push ebp
    mov ebp, esp

    x86_enterRealMode
    [bits 16]

    ; Save registers
    push bx
    push di
    push esi
    push es

    ; [bp + 20]  - &numSectors       (4 bytes)
    ; [bp + 16]  - &numHeads         (4 bytes)
    ; [bp + 12]  - &numCylinders     (4 bytes)
    ; [bp +  8]  - driveNumber       (4 bytes)
    ; [bp +  4]  - return address    (4 bytes)
    ; [bp +  0]  - old call frame    (4 bytes)

    mov ah, 08h         ; AH = BIOS function: Read drive parameters
    mov dl, [bp + 8]    ; DL = drive number
    mov di, 0           ; Set ES:DI = 0
    mov es, di
    stc 
    int 13h

    ; CF set on error
    ; DH index of last head. Add 1 since heads start at 0
    ; CX[7:6] [15:8] index of last cylinder. Add 1 since cylinders start at 0
    ; CX[5:0]        index of last sector. Fine as is as sectors start at 1
    ; CX =       ---CH--- ---CL---
    ; cylinder : 76543210 98
    ; sector   :            543210
    ; Also modifies AH, DL, BL, and ES:DI

    ; return success status
    mov eax, 1
    sbb eax, 0       ; Subtract with borrow. If CF is clear then EAX = 1, otherwise EAX = 0

    ; numCylinders
    mov bl, ch
    mov bh, cl
    shr bh, 6
    inc bx
    linearToSegmented [bp + 12], es, esi, si
    mov [es:si],bx

    ; numSectors
    xor ch,ch
    and cl,0x3F
    linearToSegmented [bp + 20], es, esi, si
    mov [es:si], cx

    ; numHeads
    mov cl, dh
    inc cx
    linearToSegmented [bp + 16], es, esi, si
    mov [es:si], cx

    ; Restore registers
    pop es
    pop esi
    pop di
    pop bx

    push eax
    x86_enterProtectedMode
    [bits 32]
    pop eax

    ; Restore old stack frame
    mov esp, ebp
    pop ebp
    ret

;
; Bool _cdecl bios_resetDisk(Uint8 driveNumber)
;
; Resets the specified drive
;
; Returns true/false on success/failure
;

global bios_resetDisk
bios_resetDisk:
    [bits 32]

    ; Make new stack frame
    push ebp
    mov ebp, esp

    x86_enterRealMode
    [bits 16]

    ; [bp + 0] - old call frame
    ; [bp + 4] - return address
    ; [bp + 8] - drive number
 
    mov ah, 0           ; AH = BIOS function: Reset disk
    mov dl, [bp + 8]    ; DL = drive number
    stc
    int 13h             ; Int 13h AH=0: Reset disk

    ; return success status
    mov eax, 1
    sbb eax, 0       ; Subtract with borrow. If CF is clear then EAX = 1, otherwise EAX = 0

    push eax
    x86_enterProtectedMode
    [bits 32]
    pop eax

    ; Restore old stack frame
    mov esp, ebp
    pop ebp
    ret

;
; Bool _cdecl bios_readDisk(
;                 Uint8  driveNumber,
;                 Uint16 cylinder,
;                 Uint16 head,
;                 Uint16 sector,
;                 Uint8  count,
;                 Uint8* buffer,
;                 Uint8* status);
;
; Reads count sectors starting at cylinder/head/sector of the specified drive into buffer
;
; Returns
;   function returns success or failure
;   *status will contain the status
;   data will be at location pointed to by buffer
;
; NOTES:
;   count is a Uint8, so it has a max value of 255
;
;   Given the way the ES:BX is calculated from buffer, BX + count should never cross the segment boundary
;
; Bochs will fail when count > 72. It sets CF, returns 1 in AH (invalid param) and 0 in AL
; Bochs will fail when destination end offset > ???. It sets CF, returns 0 in AL and 9 in AH (data boundary error)
; Qemu will fail if count > 128. It sets CF, returns 1 in AH (invalid param) and leaves AL unchanged
; Qemu will fail when destination end offset > ~FB00. It sets CF, returns 0 in AL and 9 in AH (data boundary error)
;
; I did have this function take a pointer to count so as to return the actual count read
; I've found no evidence of a successful read returning anything other than the requested count
; It was pain for the caller to have to create a temporary variable to hold the count
; SO reverted back to passing in count directly
;

global bios_readDisk
bios_readDisk:
    [bits 32]

    ; Make new stack frame
    push ebp
    mov ebp, esp

    x86_enterRealMode
    [bits 16]

    ; Save registers
    push ebx
    push es

    ; [bp + 32] - *status           (4 bytes)
    ; [bp + 28] - *buffer           (4 bytes)
    ; [bp + 24] - count             (4 bytes)
    ; [bp + 20] - sector            (4 bytes)
    ; [bp + 16] - head              (4 bytes)
    ; [bp + 12] - cylinder          (4 bytes)
    ; [bp +  8] - driveNumber       (4 bytes)
    ; [bp +  4] - return address    (4 bytes)
    ; [bp +  0] - old call frame    (4 bytes)

    ;   AH = 02h
    ;   AL = Number of sectors to read
    ;   CX[7:6] [15:8] cylinder. Cylinders start at 0
    ;   CX[5:0]        Sector. Sectors start at 1
    ;       CX =       ---CH--- ---CL---
    ;       cylinder : 76543210 98
    ;       sector   :            543210
    ;   DH = head   Heads start at 0
    ;   DL = drive
    ;   ES:BX = Destination address

    mov dl, [bp + 8]    ; drive

    mov ch, [bp + 12]   ; cylinder
    mov cl, [bp + 13]
    shr cl, 6

    mov dh, [bp + 16]   ; head

    mov al, [bp + 20]   ; sector
    and al, 0x3F
    or cl, al

    mov al, [bp + 24]   ; count

    linearToSegmented [bp + 28], es, ebx, bx    ; buffer

    mov ah, 0x2         ; Disk read function

    stc
    int 13h

    ;   CF: Set on error, clear if no error
    ;   AH = Return code
    ;   AL = Number of actual sectors read. Always seems to be 0 or requested count

    linearToSegmented [bp + 32], es, ebx, bx    ; Update status
    mov [es:bx], ah

    ; return success status
    mov eax, 1
    sbb eax, 0           ; Subtract with borrow. If CF is clear then EAX = 1 (true), otherwise EAX = 0 (false)
 
    ; Restore registers
    pop es
    pop ebx

    push eax
    x86_enterProtectedMode
    [bits 32]
    pop eax

    ; Restore old stack frame
    mov esp, ebp
    pop ebp
    ret

;################################
; Extended functions
;################################

;
; Bool bios_hasDiskExtensions(Uint8 driveNumber);
;
; Returns true if the BIOS disk extensions are enabled for the specified drive
;

global bios_hasDiskExtensions
bios_hasDiskExtensions:
    [bits 32]

    ; Make new stack frame
    push ebp
    mov ebp, esp

    x86_enterRealMode
    [bits 16]

    ; Save registers
    push bx
    push dx

    ; [bp +  8]  - driveNumber       (4 bytes)
    ; [bp +  4]  - return address    (4 bytes)
    ; [bp +  0]  - old call frame    (4 bytes)

    mov ah, 0x41        ; BIOS function
    mov bx, 0x55AA      ; Magic number
    mov dl, [bp + 8]    ; DL = drive number
    stc
    int 13h             ; Clear CF => has extensions
    mov eax, 1
    sbb eax, 0          ; Subtract with borrow. If CF is clear then EAX = 1, otherwise EAX = 0

    pop dx
    pop bx

    push eax
    x86_enterProtectedMode
    [bits 32]
    pop eax

    ; Restore old stack frame
    mov esp, ebp
    pop ebp
    ret

;
; Bool bios_getExtDriveParams(Uint16* bytesPerSector)
;
; Gets the bytes per sector of the specified drive
;
; Returns true/false on success/failure
;

    ; Extensions Drive Parameter block
edp:
    .size:              dw 0x1A
    .flags:             dw 0
    .cylinders:         dd 0
    .heads:             dd 0
    .sectors:           dd 0
    .totalSectors:      dq 0
    .bytesPerSector:    dw 0

global bios_getExtDriveParams
bios_getExtDriveParams:
    [bits 32]

    ; Make new stack frame
    push ebp
    mov ebp, esp

    x86_enterRealMode
    [bits 16]

    ; Save registers
    push ebx
    push si
    push ds
    push es

    ; [bp + 12]  - &bytesPerSector   (4 bytes)
    ; [bp +  8]  - driveNumber       (4 bytes)
    ; [bp +  4]  - return address    (4 bytes)
    ; [bp +  0]  - old call frame    (4 bytes)

    mov ah, 48h         ; AH = BIOS function: Read drive extended parameters
    mov dl, [bp + 8]    ; DL = drive number

    lea si, [edp]       ; Load edp address into DS:SI
    ; shr si, 4
    ; mov ds, si          ; DS = &edp >> 4
    ; lea si, [edp]
    ; and si, 0xF         ; SI = &edp & 0xF

    stc 
    int 13h

    ; return success status
    mov eax, 1
    sbb eax, 0       ; Subtract with borrow. If CF is clear then EAX = 1, otherwise EAX = 0

    xor bx,bx
    mov ds, bx
    linearToSegmented [bp + 12], es, ebx, bx    ; Update bytesPerSector
    mov cx, [edp.bytesPerSector]
    mov [es:bx], cx

    ; Restore registers
    pop es
    pop ds
    pop si
    pop ebx

    push eax
    x86_enterProtectedMode
    [bits 32]
    pop eax

    ; Restore old stack frame
    mov esp, ebp
    pop ebp
    ret

;
;
; Bool bios_ExtReadDisk(Uint8 id, Uint32 lba, Uint16 count, Uint8* buff, Uint8* status)
;
; Reads count sectors starting at cylinder/head/sector of the specified drive into buffer
;
; Returns
;   function returns success or failure
;   *status will contain the status
;   data will be at location pointed to by buffer
;
; Notes:
;   Read from disk using extended read
;   This allows directly specifying an LBA as well as a much larger count (Uint16) and lba (Uint64)
;

dap:
    .size:      db 0x10
                db 0
    .count:     dw 0
    .offset:    dw 0
    .segment:   dw 0
    .lba_low:    dd 0
    .lba_high:   dd 0

global bios_ExtReadDisk
bios_ExtReadDisk:
    [bits 32]

    ; Make new stack frame
    push ebp
    mov ebp, esp

    x86_enterRealMode
    [bits 16]

    ; Save registers
    push ebx
    push edx
    push es

    extern biosHasDiskExtension
    mov eax, [biosHasDiskExtension]
    cmp eax, 0
    je .noExtensions

    ; [bp + 28] - *status           (4 bytes)
    ; [bp + 24] - *buffer           (4 bytes)
    ; [bp + 20] - count             (4 bytes)
    ; [bp + 12] - lba               (8 bytes)
    ; [bp +  8] - driveNumber       (4 bytes)
    ; [bp +  4] - return address    (4 bytes)
    ; [bp +  0] - old call frame    (4 bytes)

    ;   AH    = 42h
    ;   DL    = drive
    ;   DS:SI = Address of Disk Access Packet

    mov dl, [bp + 8]                            ; drive

    mov eax, [bp + 12]                          ; lba      - 64 bit copy
    mov [dap.lba_low], eax
    mov eax, [bp + 16]
    mov [dap.lba_high], eax

    mov ax, [bp + 20]                           ; count
    mov [dap.count], ax

    linearToSegmented [bp + 24], es, ebx, bx    ; buffer
    mov [dap.offset], bx
    mov [dap.segment], es

    mov ah, 0x42         ; Disk read function
    mov si, dap

    stc
    int 13h

    ;   CF: Set on error, clear if no error
    ;   AH = Return code
    ;   AL = Number of actual sectors read. Always seems to be 0 or requested count

    linearToSegmented [bp + 28], es, ebx, bx    ; Update status
    mov [es:bx], ah

    ; return success status
    mov eax, 1
    sbb eax, 0           ; Subtract with borrow. If CF is clear then EAX = 1 (true), otherwise EAX = 0 (false)

.noExtensions
    ; Restore registers
    pop es
    pop edx
    pop ebx

    push eax
    x86_enterProtectedMode
    [bits 32]
    pop eax

    ; Restore old stack frame
    mov esp, ebp
    pop ebp
    ret

