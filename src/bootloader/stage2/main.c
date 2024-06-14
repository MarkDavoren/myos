#include "stdtypes.h"
#include "stdio.h"
#include "disk.h"

void end();

Uint8 far* buffer = ((void far*) 0x00508000);

void _cdecl cstart_(Uint16 bootDrive)
{
    Disk disk;
    Uint32 lba = 0;
    Uint8 count = 4;

    if (!diskInit(&disk, bootDrive)) {
        printf("Failed to init disk\r\n");
        end();
    }

    printf("id = %d\r\n", disk.id);
    printf("#cylinders = %d\r\n", disk.numCylinders);
    printf("#heads = %d\r\n", disk.numHeads);
    printf("#sectors = %d\r\n", disk.numSectors);

    if (!diskRead(&disk, lba, &count, buffer)) {
        printf("Failed to read disk\r\n");
        end();
    }

    printf("count = %d\r\n", count);

    for (int ii = 0; ii < 16; ii++) {
        printf("%x ", buffer[ii]);
    }
    printf("\r\n");

    end();
}

void end() {
    for (;;)
        ;
}
