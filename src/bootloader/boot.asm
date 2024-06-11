org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

;
; https://wiki.osdev.org/FAT#FAT_12
;
; FAT12 Header
;
    jmp short start
    nop

    bpb_oem:                    db 'MSWIN4.1'   ; 8 bytes
    bpb_bytes_per_sector:       dw 512
    bpb_sectors_per_cluster:    db 1
    bpb_reserved_sectors:       dw 1
    bpb_fat_count:              db 2
    bpb_dir_entries_count:      dw 0xE0
    bpb_total_sectors:          dw 2880         ; 2880 * 512 = 1.44MB
    bpb_media_descriptor_type:  db 0xF0         ; F0 = 3.5" floppy disk
    bpb_sectors_per_fat:        dw 9
    bpb_sectors_per_track:      dw 18
    bpb_head_count:             dw 2
    bpb_hidden_sectors:         dd 0
    bpb_large_sector_count:     dd 0

; Extended Boot Record
    ebr_drive_number:           db 0                    ; 0x00 = floppy 1
                                db 0                    ; reserved
    ebr_signature:              db 0x29                 ; Magic number
    ebr_volume_id:              db 12h, 34h, 56h, 78h   ; value doesn't matter
    ebr_volume_label:           db 'MYOS       '        ; 11 bytes
    ebr_system_id               db 'FAT12   '           ; 8 bytes

;
; Start of code
;
start:
    jmp main

;
; Print a null terminated string to the screen
; Parameters:
;   - ds:si: Address of null-terminated string to be printed
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
    int 0x10            ; Call bios interrupt to display AL

    jmp .loop

.done:
    pop bx
    pop ax
    pop si   

    ret

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

;
; Main code
; Parameters:
;   dl: boot drive number
;
main:
    ; Can't assume registers are set to anything useful except for DL

    ; Set data segment registers to zero
    mov ax, 0           ; Can't set ds/es directly
    mov ds, ax
    mov es, ax
    
    ; Setup stack
    mov ss, ax          ; Set stack segment register to zero
    mov sp, 0x7C00      ; Place stack below code which starts at 0x7C00. Stack goes down

    cld                 ; clear direction flag so lodsb moves forward

    ; print hello world message
    mov si, hello_message
    call puts

    mov [ebr_drive_number], dl

    mov ax, 1
    mov cl, 1
    mov bx, 0x7E00      ; This boot block is placed in 0x7C00-0x7DFF
    call read_disk

    mov si, goodbye_message
    call puts

    jmp halt

;
; Convert LBA (Logical Block Address) disk address to CHS (Cylinder/Head/Sector) address
; Parameters:
;   - ax: LBA adress
; Returns:
;   - cx [bits 0-5]: sector
;   - cx [bits 6-15]: cylinder
;   - dh: head
; Comment:
;   Return values are in the registers need to make BIOS disk read call
;
;   CHS is a standard way of referencing blocks (sectors) on a floppy disk
;   It divides the disk into cylinders
;   Each cylinder is divided into tracks by the head number
;   Each track is divided into sectors
;   CHS assumes a fixed number of sectors (blocks) in a track which is not the case in modern disks
;   Cylinders and heads start at zero. Sectors start at 1 
;
;   LBA treats disk as an array of sectors starting at block 0
;
;   To convert:
;       sector   = (LBA % sectors_per_track) + 1
;       cylinder = (LBA / sectors_per_track) / number_of_heads
;       head     = (LBA / sectors_per_track) % number_of_heads
;
;   The BIOS disk read function Int 13h AH=02h requires that the
;   cylinder and sector are encoded in CX as
;       CX =       ---CH--- ---CL---
;       cylinder : 76543210 98
;       sector   :            543210
lba_to_chs:
    push ax
    push dx    ; we want to restore dh. dl will contain a return value

    ; The DIV instruction combines DX and AX into a 32 dividend
    ; It performs an integer division and places and
    ; the integer quotient into AX
    ; the remainder into DX
    ; We need to zero out DX since we are just provided with AX
    xor dx, dx
    div word [bpb_sectors_per_track]
    ; AX = LBA / sectors_per_track
    ; DX = LBA % sectors_per_track

    inc dx          ; sector = (LBA % sectors_per_track) + 1
    mov cx, dx      ; cx = sector

    xor dx, dx
    div word [bpb_head_count]
    ; AX = (LBA / sectors_per_track) / number_of_heads, i.e., cylinder
    ; DX = (LBA / sectors_per_track) % number_of_heads, i.e., head

    mov dh, dl      ; dh = head
    mov ch, al      ; ch = lower 8 bits of cylinder
    shl ah, 6       ; Bits 8 & 9 of cylinder are in AH bits 0 & 1. Shift them to 6 & 7
    or cl, ah       ; and OR them into CL

    pop ax
    mov dl, al      ; restore DL
    pop ax          ; restore AX

    ret

;
; Read sectors from a disk
; Parameters:
;   - ax: LBA
;   - cl: number of sectors to read (up to 128)
;   - dl: drive number
;   - es:bx: address of buffer to hold data read from disk
;
;
; Comments:
;   It is recommended to retry reads from disk several times on failure (3)
;   with a reset after each failure
;
read_disk:
    push ax
    push bx
    push cx
    push dx
    push di

    push cx             ; Save CL (Can't push byte?)
    call lba_to_chs     ; Returns CHS in CX and DH
    pop ax              ; AL = number of sectors to read

    mov ah, 02h         ; Setup for BIOS call Int 13h AH=02h
    mov di,3            ; retry count

.retry
    pusha               ; push all registers as we don't know what BIOS will change
    stc                 ; set carry flag. Some BIOS's don't set it on error

    ; Int 13h AH=02h
    ; Parameters:
    ;   AH = 02h
    ;   AL = Number of sectors to read
    ;   CX = cylinder [6-15] & sector [0-5]
    ;   DH = head
    ;   DL = drive
    ;   ES:BX = Destination address
    ; Returns
    ;   CF: Set on error, clear if no error
    ;   AH = Return code
    ;   AL = Number of actual sectors read
    int 13h
    jnc .done           ; if no error (carry flag clear) then jmp to done

    ; read failed
    popa
    call reset_disk

    dec di
    test di, di
    jnz .retry

    ; Reached retry limit
    mov si, read_disk_failure_msg
    call puts
    jmp wait_for_key_and_reboot

.done
    popa

    pop di
    pop dx
    pop cx
    pop bx
    pop ax

    ret

;
; Reset disk
; Parameters:
;   dl: drive number
;
reset_disk:
    pusha
    mov ah, 0
    stc
    int 13h             ; Int 13h AH=0: Reset disk. DL = drive number
    jc .disk_reset_fail
    popa
    ret

.disk_reset_fail:
    mov si, reset_disk_failure_msg
    call puts
    jmp wait_for_key_and_reboot

wait_for_key_and_reboot:
    mov ah, 0
    int 16h
    jmp 0xFFFF:0        ; Jump to beginning of BIOS which should cause a reboot
    ; Should never get here, but if we do then conveniently halt is next

;
; Halt processing
; Parameters:
;   None
;
halt:
    cli                 ; disable interrupts which would break CPU out of halted state
    hlt

.halt:
    jmp .halt           ; Just in case CPU breaks out of hlt

; Strings

hello_message: db 'Hello world!', ENDL, 0
goodbye_message: db 'Farewell Cruel World!', ENDL, 0
read_disk_failure_msg: db 'Read from disk failed!', ENDL, 0
reset_disk_failure_msg: db 'Disk reset failed!', ENDL, 0

times 510-($-$$) db 0
dw 0xAA55
