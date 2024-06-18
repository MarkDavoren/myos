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
 * This space is freed up once the jumpt to stage 2 occurs
 *
 * -------------------------
 * 
 * we have dedicated some of the free space as follows
 * 
 *   0x00000000 - 0x000003FF - interrupt vector table
 *   0x00000400 - 0x000004FF - BIOS data area
 *
 *   0x00000500 - 0x000104FF - FAT driver data
 *   0x00010500 - 0x0001FFFF - Free
 *   0x00020000 - 0x00030000 - Stage2 - Stack goes down from 0x30000
  *  0x00030000 - 0x00080000 - Free
 *
 *   0x00080000 - 0x0009FFFF - Extended BIOS data area
 *   0x000A0000 - 0x000C7FFF - Video
 *   0x000C8000 - 0x000FFFFF - BIOS
 * 
 *   0x00100000 - 0x00110000 - Kernel
 */

#define FAT_MEM_ADDRESS     ((void*) 0x20000)
#define FAT_MEM_SIZE        0x10000

#define SCRATCH_MEM_ADDRESS ((void*) 0x30000)
#define SCRATCH_MEM_SIZE    0x10000

#define KERNEL_LOAD_ADDR    ((void*) 0x100000)

