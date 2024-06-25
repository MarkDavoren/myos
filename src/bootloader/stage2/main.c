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

Partition partitionTable[4];

void __attribute__((cdecl)) start(Uint16 bootDrive, void* pt)
{
    Bool ok;
    clearScreen();
    printf("Hello from Stage2. Boot drive = %x\n", bootDrive);

    // Copy partition table into a safe, known location
    Partition* pp = (Partition*)pt;
    for (int ii = 0; ii < 4; ++ii) {
        partitionTable[ii] = *(Partition*) pt;
        pt += sizeof(Partition);
    }

    printPartitionTable(partitionTable);

    ok = fatInitialize(bootDrive, partitionTable);  // TODO: Assumes using first partition
    if (!ok) {
        panic("fatInitialize returned false");
    }
    printf("fatInitialize: ok = %d\n", ok);

    Handle fin = fatOpen("/kernel.bin");
    //Handle fin = fatOpen("/mydir/test.txt");
    if (fin == BAD_HANDLE) {
        panic("fatOpen returned error");
    }
    printf("fatOpen: fin = %u\n", fin);

    // char buff[3000];
    // Uint32 count = fatRead(fin, 3000, buff);
    // printf("count = %d\n%s", count, buff);
    // panic("Done reading /mydir/test.txt");
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
