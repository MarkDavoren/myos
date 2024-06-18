#include "stdtypes.h"
#include "stdio.h"
#include "x86.h"
#include "disk.h"
#include "fat.h"
#include "utility.h"

void end();


void printFileContents(Handle fin);

Uint8* dbuff = (Uint8*) 0x20000;

void __attribute__((cdecl)) start(Uint16 bootDrive)
{
    Bool ok;
    clearScreen();
    printf("Hello\n");
    
    ok = fatInitialize(bootDrive);
    printf("fatInitialize: ok = %d\n", ok);

    Handle fin = fatOpen("/mydir/test.txt");

    printFileContents(fin);

    end();
}

void printFileContents(Handle fin)
{
    if (fin == BAD_HANDLE) {
        printf("Bad handle\n");
        end();
    }

    char buff[100];
    Uint32 count;
    while ((count = fatRead(fin, 100, buff)) > 0) {
        buff[count] = '\0';
        printf("%s\n", buff);
    }
}
