#include "stdtypes.h"
#include "stdio.h"
#include "disk.h"
#include "fat.h"

void end();

Uint8 far* buffer = ((void far*) 0x00508000);

void _cdecl cstart_(Uint16 bootDrive)
{
    fatInitialize(bootDrive);

    printf("FAT initialized\r\n");

    Handle fin = fatOpen("/test.txt");

    char buff[100];
    Uint32 count = fatRead(fin, 100, buff);
    buff[count] = '\0';
    printf("Count = %ld, Contents = '%s'\r\n", count, buff);
    
    end();
}

void end() {
    for (;;)
        ;
}
