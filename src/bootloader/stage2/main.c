#include "stdtypes.h"
#include "stdio.h"
#include "x86.h"
#include "disk.h"
#include "fat.h"
#include "utility.h"
#include "memdefs.h"
#include "string.h"
#include "mbr.h"

typedef void (*KernelStart)();
void jumpToKernel();

void __attribute__((cdecl)) start(Uint16 bootDrive, void* partitionTable)
{
    Bool ok;
    clearScreen();
    printf("Stage1\n");
    printf("Hello from stage 2 boot drive = %x, partition table at %x\n", bootDrive, partitionTable);

    printPartitionTable(partitionTable);

    ok = fatInitialize(bootDrive, partitionTable);  // TODO: Assuming use first partition. Need fix for floppy
    printf("fatInitialize: ok = %d\n", ok);

    Handle fin = fatOpen("/kernel.bin");
    printf("fatOpen: fin = %u\n", fin);

    Uint8* kp = KERNEL_LOAD_ADDR;
    Uint32 count;
    while ((count = fatRead(fin, SCRATCH_MEM_SIZE, SCRATCH_MEM_ADDRESS)) > 0) {
        printf("Copying %x bytes from %p to %p\n", count, SCRATCH_MEM_ADDRESS, kp);
        memcpy(kp, SCRATCH_MEM_ADDRESS, count);
        kp += count;
    }

    fatClose(fin);

    jumpToKernel();
}

void jumpToKernel()
{
    Uint8* pp = KERNEL_LOAD_ADDR;
    for (int ii = 0; ii < 16; ++ii) {
        printf("%x ", pp[ii]);
    }
    printf("\n");

    KernelStart kernelStart = (KernelStart) KERNEL_LOAD_ADDR;
    printf("Jumping to kernel at %p\n", kernelStart);
    kernelStart();

}
