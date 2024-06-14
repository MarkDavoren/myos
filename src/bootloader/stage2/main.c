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
    
    end();
}

void end() {
    for (;;)
        ;
}
