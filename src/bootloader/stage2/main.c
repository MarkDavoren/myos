#include "stdtypes.h"
#include "stdio.h"
#include "disk.h"

void end();

uint8_t far* buffer = ((void far*) 0x00508000);

void _cdecl cstart_(uint16_t bootDrive)
{
    Disk disk;
    uint32_t lba = 0;
    uint8_t count = 4;

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
