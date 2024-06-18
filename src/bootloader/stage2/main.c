#include "stdtypes.h"
#include "stdio.h"
// #include "disk.h"
// #include "fat.h"
// #include "utility.h"

void end();

//Uint8 far* buffer = ((void far*) 0x00508000);

//void printFileContents(Handle fin);

void __attribute__((cdecl)) start(Uint16 bootDrive)
{
    clearScreen();

    printf("Hello from C!!\r\n");

    // fatInitialize(bootDrive);

    // printf("FAT initialized\r\n");

    // Handle fin = fatOpen("/test1.txt");

    // printFileContents(fin);

    end();
}

// void printFileContents(Handle fin)
// {
//     if (fin == BAD_HANDLE) {
//         printf("Bad handle\r\n");
//         end();
//     }

//     char buff[100];
//     Uint32 count;
//     while ((count = fatRead(fin, 100, buff)) > 0) {
//         buff[count] = '\0';
//         printf("%s\r\n", buff);
//     }
// }
