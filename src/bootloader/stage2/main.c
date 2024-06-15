#include "stdtypes.h"
#include "stdio.h"
#include "disk.h"
#include "fat.h"

void end();

Uint8 far* buffer = ((void far*) 0x00508000);

void printFileContents(Handle fin);

void _cdecl cstart_(Uint16 bootDrive)
{
    fatInitialize(bootDrive);

    printf("FAT initialized\r\n");

    Handle fin = fatOpen("/subdir/test3.txt");

    printFileContents(fin);

    end();
}

void printFileContents(Handle fin)
{
    if (fin == BAD_HANDLE) {
        printf("Bad handle\r\n");
        end();
    }

    char buff[100];
    Uint32 count;
    while ((count = fatRead(fin, 100, buff)) == 100) {
        buff[count] = '\0';
        printf("%s\r\n", buff);
    }
}

void end() {
    for (;;)
        ;
}
