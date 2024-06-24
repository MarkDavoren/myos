org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

;
; This file is compiled into a 512 byte raw image.
; It is used as the Master Boot Record of a floppy disk
; As such it must follow certain rules.
;   - It must be exactly 512 bytes long
;   - It must end with 0xAA55
;   - It must start with the BIOS Parameter Block
;   - It must expect to be loaded at 0x0000:0x7C00
;   - It must expect to start execution at 0x0000:0x7C00
;
; The objectives of this code are:
;   - Set key registers to a known state, e.g. setup stack
;   - Load "STAGE2.BIN" from the FAT12 filesystem on floppy
;   - Jump to stage2

;
; Floppy disks used the FAT12 filesystem
;
; https://wiki.osdev.org/FAT#FAT_12
;
; A FAT12 disk is laid out as follows:
;   - The Reserved area starting with the Master Boot Record
;   - The File Allocation Tables (FAT)
;   - The Root directory
;   - The Data
;
; The Master Boot Record is laid out as follows:
;   - The BIOS Parameter Block
;   - The Extended BIOS Parameter Block
;   - The boot code
;   - the boot signature: 0xAA55
;
; BIOS Parameter Block
; These parameters describe the layout of the disk and the FAT12 filesystem
; The disk is structured using a CHS (Cylinder/Head/Sector) layout
; Using the values specified below:
;   bpb_total_sectors               2880
;   bpb_bytes_per_sector,           512 bytes. 2880 * 512 = 1.44MB
;   bpb_sectors_per_track,          18
;   bpb_heads                       2.   2880 / (2 * 18) = 80 cylinders
; So
;   cylinders = 0..79
;   heads = 0..1
;   sectors = 1..18
; Note: The media may be moved to a machine with a different BIOS which may
;   specify a different geometry. So you should not trust SPT or heads. Get them
;   from the BIOS
; The BPB is followed by the Extended Boot Record (EBR) which contains
;   little of interest to us. We will get the actual drive number from
;   the BIOS and store it in ebr_drive_number
;
; Disk Layout
; The disk has the following areas
;   Section                 Size in sectors
;   Reserved sectors        bpb_reserved_sectors
;   FATs                    bpb_fat_count * bpb_sectors_per_fat
;   The Root directory      roundup(bpb_dir_entries_count * 32 / bpb_bytes_per_sector)
;   The Data                The remainder of the disk
;
; Root directory
; Consists bpb_dir_entries_count entries, each structured as follows
;   Offset  Length  Meaning
;   0       11      8.3 file name
;   11      1       File attributes
;   12      1       Reserved
;   13      1       Creation time hundredths of a second
;   14      2       Creation time
;   16      2       Creation date
;   18      2       Last accessed date
;   20      2       High 16 bits of first cluster number. Always zero in FAT12
;   22      2       Last modification time
;   24      2       Last modification date
;   26      2       Low 16 bits of first cluster number
;   28      4       Size of file in bytes
;
; File Allocation Tables
; Typically there are two FATs in the disk, but the second is a backup copy and is usually ignored
;
; The FAT12 filesystem stores files as a sequence of clusters of size bpb_sectors_per_cluster sectors
; The directory entry for a file contains its starting cluster number
;
; In FAT12, 12 bits are used to represent a cluster number
; The FAT is a table of cluster numbers. Use the starting cluster number
; as an index into the FAT to find the next cluster number of the file
; Use that number to find the next and so forth until you reach a cluster number >= 0xFF8
;   

;
; BIOS Parameter Block
;
    jmp short start                             ; 3 bytes to jump to main code
    nop                                         ;

    bpb_oem:                    db 'MSWIN4.1'   ; 8 bytes - ignored
    bpb_bytes_per_sector:       dw 512
    bpb_sectors_per_cluster:    db 1
    bpb_reserved_sectors:       dw 1
    bpb_fat_count:              db 2
    bpb_dir_entries_count:      dw 0xE0         ; 224 decimal
    bpb_total_sectors:          dw 3840         ; 30 * 4 * 32
    bpb_media_descriptor_type:  db 0xF0         ; F0 = 3.5" floppy disk
    bpb_sectors_per_fat:        dw 9
    bpb_sectors_per_track:      dw 32
    bpb_heads:                  dw 4
    bpb_hidden_sectors:         dd 0
    bpb_large_sector_count:     dd 0            ; Only used if there are > 65535 sectors

; Extended Boot Record
    ebr_drive_number:           db 0                    ; 0x00 = floppy 1
                                db 0                    ; reserved
    ebr_signature:              db 0x29                 ; Magic number
    ebr_volume_id:              db 12h, 34h, 56h, 78h   ; value doesn't matter
    ebr_volume_label:           db 'MYOS       '        ; 11 bytes
    ebr_system_id:              db 'FAT12   '           ; 8 bytes

;
; Main code
; Parameters:
;   dl: boot drive number
; Note that the state of all other registers is undefined
;
start:
;
; Establish environment
;
    ; Set data segment registers to zero
    mov ax, 0           ; Can't set ds/es directly
    mov ds, ax
    mov es, ax
    
    ; Setup stack
    mov ss, ax          ; Set stack segment register to zero
    mov sp, 0x7C00      ; Place stack below code which starts at 0x7C00. Stack goes down

    cld                 ; clear direction flag so lodsb moves forward

    ; Some BIOSs might start us at 0x7C00:0x0000
    ; We want to ensure CS is 0x0000
    ; Since we can't just set CS to zero as we might jump somewhere crazy, use a trick
    push es
    push word .zero_cs
    retf                ; A far return will load both CS and IP from the stack
.zero_cs:

    mov si, hello_msg
    call puts

    ; Since the media might have been created by a different BIOS
    ; we can't trust the values for sectors_per_track and heads in the MBR
    ; Get them from the current BIOS
    mov [ebr_drive_number], dl
    push es             ; BIOS returns stuff in ES:DI
    mov ah, 08h
    int 13h
    jnc .got_disk_params
    jmp disk_error
.got_disk_params:
    pop es

    and cl, 0x3F        ; Sectors per track is stored in bits 0..5
    xor ch, ch
    mov [bpb_sectors_per_track], cx

    inc dh
    mov [bpb_heads], dh ; BIOS sets DH to index of last head (zero indexed). Add one to get head count

;
; Load stage 2
;
;   - ax: LBA
;   - cl: number of sectors to read (up to 128)
;   - dl: drive number
;   - es:bx: address of buffer to hold data read from disk

    mov ax, 1                       ; Start reading from sector 1
    mov cl, [stage2_sector_size]    ; The size is set during the install process
    mov bx, STAGE2_LOAD_SEGMENT     ; Load into memory at STAGE2_LOAD_SEGMENT:STAGE2_LOAD_OFFSET
    mov es, bx
    mov bx, STAGE2_LOAD_OFFSET
    mov dl, [ebr_drive_number]      ; drive number
    call read_disk

    ; Copy the partition table to somewhere safe
    mov si, partition_table
    mov ax, PARTITION_ENTRY_SEGMENT
    mov es, ax
    mov di, PARTITION_ENTRY_OFFSET
    mov cx, 16
    rep movsd                       ; Copy CX dwords from DS:SI to ES:DI

    mov dl, [ebr_drive_number]      ; Pass the boot drive number to stage 2
    mov si, PARTITION_ENTRY_OFFSET  ; Pass Partition table seg:off in di:si (di not ds!)
    mov di, PARTITION_ENTRY_SEGMENT

    mov ax, STAGE2_LOAD_SEGMENT     ; Set segment registers for stage 2
    mov ds, ax
    mov es, ax

    jmp STAGE2_LOAD_SEGMENT:STAGE2_LOAD_OFFSET

    jmp wait_for_key_and_reboot

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
    div word [bpb_heads]
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

.retry:
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
    jmp disk_error

.done:
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
    jc disk_error
    popa
    ret

disk_error:
    mov si, disk_failure_msg
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

disk_failure_msg: db 'Disk operation failed!', ENDL, 0
hello_msg: db 'Hello', ENDL, 0

; Load the stage2 into memory starting at 0x0500
STAGE2_LOAD_SEGMENT equ 0x0
STAGE2_LOAD_OFFSET  equ 0x500

; Copy the partition table to 0x20000
PARTITION_ENTRY_SEGMENT equ 0x2000
PARTITION_ENTRY_OFFSET  equ 0x0

times 512 -2 -4 -2 -64 -2 -($-$$) db 0

; These data structures go at the end of the boot sector so the previous line adds just teh right amount of padding
pad: db 0
global stage2_sector_size
stage2_sector_size: db 0            ; Size of stage 2 in sectors (non-standard)
UDID: dd 0x12345678                 ; Unique Disk ID
Reserved: dw 0                      ; Reserved
partition_table: times 64 db 0      ; Partition table. 4 entries each 16 bytes long
boot_sig: dw 0xAA55                 ; The boot signature without which the BIOS won't recognize this as a boot sector

