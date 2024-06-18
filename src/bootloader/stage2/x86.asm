bits 16

section _TEXT

global x86_outb
x86_outb:
    [bits 32]
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

global x86_inb
x86_inb:
    [bits 32]
    mov dx, [esp + 4]
    xor eax, eax
    in al, dx
    ret
;
; void _cdecl bios_putc(char c, Uint8 page)
;
; Uses Int 10 0E to display c on page
;

global bios_putc
bios_putc:
    ; Make new stack frame
    push bp
    mov bp, sp

    ; Save registers
    push bx

    ; [bp + 0] - old call frame
    ; [bp + 2] - return address (small memory model => 2 bytes)
    ; [bp + 4] - first argument - character to be displayed
    ; [bp + 6] - second argument - page

    mov ah, 0Eh         ; AH = BIOS sub-function
    mov al, [bp + 4]    ; AL = character to be displayed
    mov bh, [bp + 6]    ; BH = page
    int 10h

    ; Restore registers
    pop bx

    ; Restore old stack frame
    mov sp, bp
    pop bp
    ret

;
; Bool _cdecl bios_resetDisk(Uint8 driveNumber)
;
; Uses Int 13h 0 to reset disk driveNUmber
;

global bios_resetDisk
bios_resetDisk:
    ; Make new stack frame
    push bp
    mov bp, sp

    ; [bp + 0] - old call frame
    ; [bp + 2] - return address (small memory model => 2 bytes)
    ; [bp + 4] - drive number
 
    mov ah, 0           ; AH = BIOS function: Reset disk
    mov dl, [bp + 4]    ; DL = drive number
    stc
    int 13h             ; Int 13h AH=0: Reset disk

    ; return success status
    mov ax, 1
    sbb ax, 0       ; Subtract with borrow. If CF is clear then AX = 1, otherwise AX = 0

    ; Restore old stack frame
    mov sp, bp
    pop bp
    ret

;
; Bool _cdecl bios_readDisk(
;                 Uint8  driveNumber,
;                 Uint16 cylinder,
;                 Uint16 head,
;                 Uint16 sector,
;                 Uint8  count,
;                 Uint8 far* buffer
;                 Uint8* status);
;
; Reads count sectors starting at cylinder/head/sector of the specified drive into buffer
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
; Returns
;   function returns success or failure
;   *count will contain the number sectors read
;   *status will contain the status
;   data will be at location pointed to by buffer
;

global bios_readDisk
bios_readDisk:
    ; Make new stack frame
    push bp
    mov bp, sp

    ; Save registers
    push bx
    push si
    push es

    ; [bp + 18] - *status           (2 bytes)
    ; [bp + 14] - *buffer           (4 bytes)
    ; [bp + 12] - count             (2 bytes)
    ; [bp + 10] - sector            (2 bytes)
    ; [bp + 8]  - head              (2 bytes)
    ; [bp + 6]  - cylinder          (2 bytes)
    ; [bp + 4]  - driveNumber       (2 bytes)
    ; [bp + 2]  - return address    (2 bytes)
    ; [bp + 0]  - old call frame    (2 bytes)

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

    mov dl, [bp + 4]    ; drive

    mov ch, [bp + 6]    ; cylinder
    mov cl, [bp + 7]
    shr cl, 6

    mov dh, [bp + 8]    ; head

    mov al, [bp + 10]   ; sector
    and al, 0x3F
    or cl, al

    mov al, [bp + 12]   ; count

    mov bx, [bp + 16]   ; buffer
    mov es, bx
    mov bx, [bp + 14]

    mov ah, 0x2         ; Disk read function

    stc
    int 13h

    ;   CF: Set on error, clear if no error
    ;   AH = Return code
    ;   AL = Number of actual sectors read. Always seems to be 0 or requested count

    mov si, [bp + 18]   ; Update *status
    mov [si], ah

    ; return success status
    mov ax, 1
    sbb ax, 0           ; Subtract with borrow. If CF is clear then AX = 1 (true), otherwise AX = 0 (false)
 
    ; Restore registers
    pop bx
    pop si
    pop es

    ; Restore old stack frame
    mov sp, bp
    pop bp
    ret

;
; Bool _cdecl bios_getDriveParams(
;                   Uint8 driveNumber,
;                   Uint16* numCylinders,
;                   Uint16* numHeads,
;                   Uint16* numSectors)
;
; Uses Int 13h 08 to obtain the geometry of the specified drive
; Returns true/false on success/failure
;

global bios_getDriveParams
bios_getDriveParams:
    ; Make new stack frame
    push bp
    mov bp, sp

    ; Save registers
    push bx
    push di
    push si
    push es

    ; [bp + 10] - &numSectors       (2 bytes)
    ; [bp + 8]  - &numHeads         (2 bytes)
    ; [bp + 6]  - &numCylinders     (2 bytes)
    ; [bp + 4]  - driveNumber       (2 bytes)
    ; [bp + 2]  - return address    (2 bytes)
    ; [bp + 0]  - old call frame    (2 bytes)

    mov ah, 08h         ; AH = BIOS function: Read drive parameters
    mov dl, [bp + 4]    ; DL = drive number
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
    mov ax, 1
    sbb ax, 0       ; Subtract with borrow. If CF is clear then AX = 1, otherwise AX = 0

    ; numCylinders
    mov bl, ch
    mov bh, cl
    shr bh, 6
    inc bx
    mov si, [bp + 6]
    mov [si],bx

    ; numSectors
    xor ch,ch
    and cl,0x3F
    mov si, [bp + 10]
    mov [si], cx

    ; numHeads
    inc dh
    mov si, [bp + 8]
    mov [si], dh

    ; Restore registers
    pop bx
    pop di
    pop si
    pop es

    ; Restore old stack frame
    mov sp, bp
    pop bp
    ret
