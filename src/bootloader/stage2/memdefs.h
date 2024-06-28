#pragma once

/*
 * The typical real mode 16 bit address space with an active BIOS is as follows:
 *
 *   0x00000000 - 0x000003FF - interrupt vector table
 *   0x00000400 - 0x000004FF - BIOS data area
 *
 *   0x00000500 - 0x0007FFFF - Free
 *
 *   0x00080000 - 0x0009FFFF - Extended BIOS data area
 *   0x000A0000 - 0x000C7FFF - Video
 *   0x000C8000 - 0x000FFFFF - BIOS
 *
 * The BIOS will start executing at 0x7C00 - our stage 1
 * This space is freed up once the jump to stage 2 occurs
 *
 * -------------------------
 * 
 * we have dedicated some of the free space as follows
 * 
 *   0x00000000 - 0x000003FF - interrupt vector table
 *   0x00000400 - 0x000004FF - BIOS data area
 *
 *   0x00000500 - 0x0001FFFF - stage2 code, data, and stack (going down from 0x20000)
 *   0x00020000 - 0x00020FFF - FAT data - new version where we cache only 2 FAT sectors
  *  0x00021000 - 0x00080000 - Free
 *
 *   0x00080000 - 0x0009FFFF - Extended BIOS data area
 *   0x000A0000 - 0x000C7FFF - Video
 *   0x000C8000 - 0x000FFFFF - BIOS
 * 
 *   0x00100000 - 0x00110000 - Kernel
 */

#define HEAP_ADDRESS        ((void*) 0x20000)
#define HEAP_SIZE           0x40000

#define DISK_BUFFER         ((void*) 0x20000)
#define DISK_BUFFER_SIZE    0x10000

#define FAT_MEM_ADDRESS     ((void*) 0x20000)
#define FAT_MEM_SIZE        0x20000

#define SCRATCH_MEM_ADDRESS ((void*) 0x50000)
#define SCRATCH_MEM_SIZE    0x10000

#define KERNEL_LOAD_ADDR    ((void*) 0x100000)

