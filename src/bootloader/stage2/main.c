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

void fatal(char* msg)
{
    printf("FATAL error %s\n:, msg");
    for (;;) ;
}

void __attribute__((cdecl)) start(Uint16 bootDrive, void* partitionTable)
{
    Bool ok;
    clearScreen();
    printf("Hello from Stage2. Boot drive = %x\n", bootDrive);

    printPartitionTable(partitionTable);

    ok = fatInitialize(bootDrive, partitionTable);  // TODO: Assumes using first partition
    if (!ok) {
        fatal("fatInitialize returned false");
    }
    printf("fatInitialize: ok = %d\n", ok);

    Handle fin = fatOpen("/kernel.bin");
    if (!ok) {
        fatal("fatOpen returned false");
    }
    printf("fatOpen: fin = %u\n", fin);

    Uint8* kp = KERNEL_LOAD_ADDR;
    Uint32 count;
    while ((count = fatRead(fin, 512 /*SCRATCH_MEM_SIZE*/, SCRATCH_MEM_ADDRESS)) > 0) {  // TODO Undo debug hack
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
