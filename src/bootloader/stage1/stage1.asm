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
    bpb_total_sectors:          dw 2880         ; 2880 * 512 = 1.44MB
    bpb_media_descriptor_type:  db 0xF0         ; F0 = 3.5" floppy disk
    bpb_sectors_per_fat:        dw 9
    bpb_sectors_per_track:      dw 18
    bpb_heads:                  dw 2
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
; Get root directory
;
    ; Compute LBA of root dir: reserved + FATs * size of FATs
    mov ax, [bpb_sectors_per_fat]
    mov bl, [bpb_fat_count]
    xor bh, bh
    mul bx              ; DX:AX = AX * BX. Given disk realities, assume DX is zero
    add ax, [bpb_reserved_sectors]
    push ax             ; Push LBA of root dir onto stack for later use

    ; Compute size of root directory in bytes
    mov ax, [bpb_dir_entries_count]
    shl ax, 5           ; ax *= 32 (the size of a directory entry)
    ; Convert that into sectors
    xor dx, dx
    div word [bpb_bytes_per_sector]  ; DX:AX / bps => AX = quotient, DX = remainder
    test dx, dx
    jz .root_dir_roundup
    inc ax                  ; round up sector count if dir size not a multple of BPS
.root_dir_roundup:

    ; Read root directory into buffer
    mov cl, al                  ; cl = size of root dir in sectors
    pop ax                      ; ax = LBA of root dir calculated above
    mov dl, [ebr_drive_number]  ; dl = drive number of disk
    mov bx, buffer              ; es:bx = read buffer
    call read_disk

;
; Search for stage2.bin
;
    xor bx, bx
    mov di, buffer              ; DI points to the first directory entry

.search_stage2:
    ; The filename is the first field in a directory entry
    mov si, stage2_filename
    mov cx, 11
    push di
    ; The repe cmpsb instruction repeats while DS:SI == ES:DI up to CX times
    ; incrementing SI and DI by 1 and decrementing CX by 1 each time
    ; ZF is set if the strings are equal
    repe cmpsb
    pop di
    je .found_stage2

    add di, 32
    inc bx
    cmp bx, [bpb_dir_entries_count]
    jl .search_stage2

    mov si, stage2_not_found_msg
    call puts
    jmp wait_for_key_and_reboot

.found_stage2:
    ; DI points to directory entry for the stage2
    mov ax, [di + 26]           ; The first cluster number of the file
    mov [cluster], ax    ; Don't have enough registers so put in memory

;
; Load FAT
;
; Note that though there are bpb_fat_count FATs, we just use the first as the others are just copies
;
    ; We've got what we want from the root directory so we can use buffer to hold the FAT
    mov ax, [bpb_reserved_sectors]  ; LBA of FAT
    mov bx, buffer                  ; read buffer
    mov cl, [bpb_sectors_per_fat]   ; number of sectors to read
    mov dl, [ebr_drive_number]      ; drive number
    call read_disk

    ; Set up a segmented address for the stage2 to be loaded into
    mov bx, STAGE2_LOAD_SEGMENT
    mov es, bx
    mov bx, STAGE2_LOAD_OFFSET

;
; Load stage2
;
    ; HACK ALERT!!!
    ; The starting sector of a cluster is
    ; cluster LBA = (cluster_number - 2) * sectors_per_cluster + start_data
    ; where start_data = Reserved sectors + FAT sectors + root dir size
    ;
    ; Given
    ;   - sectors_per_cluster = 1
    ;   - reserved sectors = 1
    ;   - FAT sectors = #FATS * sectors per FAT = 2 * 9 = 18
    ;   - root dir size = roundup(#dir entries * 32 / bytes per sector) = 224 * 32 / 512 = 14
    ; Then
    ; cluster LBA = (cluster_number - 2) * 1 + 1 + 18 + 14 = cluster_number + 31
.load_stage2_loop:
    mov ax, [cluster]
    add ax, 31
    mov cl, 1
    mov dl, [ebr_drive_number]
    call read_disk

    add bx, [bpb_bytes_per_sector]  ; !!! Not checking for buffer overrun

    ; Compute next cluster number
    ; In FAT 12, cluster numbers in the FAT are 12 bits long
    ; So multiply the cluster number by 3/2 to get a word index into the FAT
    ; If the cluster number is even, we want the bottom 12 bits
    ; Otherwise we want the top 12 bits
    ; 
    mov ax, [cluster]
    mov cx, 3
    mul cx
    mov cx, 2
    div cx          ; AX = index, DX = (cluster * 3) % 2

    mov si, buffer
    add si, ax
    mov ax, [ds:si]

    ; If DX is zero then cluster is even: get bottom 12 bits
    ; If DX is non-zero then cluster is odd: get top 12 bits
    or dx, dx
    jz .even
    shr ax, 4
    jmp .got_next_cluster
.even:
    and ax, 0x0FFF
.got_next_cluster:

    cmp ax, 0x0FF8
    jae .read_finish

    mov [cluster], ax
    jmp .load_stage2_loop

.read_finish:
    mov dl, [ebr_drive_number]

    mov ax, STAGE2_LOAD_SEGMENT
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

stage2_not_found:
    mov si, stage2_not_found_msg
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
stage2_not_found_msg: db 'STAGE2.BIN not found!', ENDL, 0

stage2_filename: db 'STAGE2  BIN'

cluster: dw 0

times 510-($-$$) db 0
dw 0xAA55

buffer:                 ; This will be at 0x7E00

; Load the stage2 into memory starting at 0x2000
STAGE2_LOAD_SEGMENT equ 0x2000
STAGE2_LOAD_OFFSET  equ 0

