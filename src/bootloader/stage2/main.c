#include "stdtypes.h"
#include "stdio.h"
#include "x86.h"
#include "disk.h"
#include "ext.h"
#include "utility.h"
#include "memdefs.h"
#include "string.h"
#include "mbr.h"
#include "alloc.h"

typedef void (*KernelStart)();
void jumpToKernel();

Partition partitionTable[4];

void __attribute__((cdecl)) start(Uint16 bootDrive, void* pt)
{
    Bool ok;
    clearScreen();
    printf("Hello from Stage2. Boot drive = %x\n", bootDrive);

    heapInit(HEAP_ADDRESS, HEAP_SIZE);
    printHeap();

   // Copy partition table into a safe, known location
    Partition* pp = (Partition*)pt;
    for (int ii = 0; ii < 4; ++ii) {
        partitionTable[ii] = *(Partition*) pt;
        pt += sizeof(Partition);
    }
    printPartitionTable(partitionTable);

    ok = extInitialize(bootDrive, partitionTable);

    Handle fin = extOpen("/test.txt");
    if (fin == BAD_HANDLE) {
        panic("extOpen returned error");
    }
    printf("extOpen: fin = %u\n", fin);

    printf("File contents:\n");
    char buff[1024 + 1];
    Uint32 count;
    while ((count = extRead(fin, 1024, buff))) {
        buff[count] = '\0';
        printf("%s", buff);
    }
    panic("Stop in main");
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
