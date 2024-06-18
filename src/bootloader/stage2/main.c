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

    int d = -3;
    int i = 4;
    long l = 1000000;
    unsigned u = 5;
    unsigned x = 0x1234567;
    unsigned X = 0x7654321;
    unsigned o = 0123456;
    unsigned* p = &u;

    printf("%%\n");
    printf("c = '%c', s = '%s'\n", 'X', "hello");
    printf("l = %d\n", l);
    printf("l = %ld\n", l);
    printf("d = %d, i = %i, l = %d, u = %u\n", d, i, l, u);
    printf("x = %x\n", x);
    printf("X = %X\n", X);
    printf("o = %o\n", o);
    printf("p = %p\n", p);
    printf("x = %x, X = %X, o = %o, p = %p\n", x, X, o, p);

    printf("char = %d\n", sizeof(char));
    printf("short = %d\n", sizeof(short));
    printf("int = %d\n", sizeof(int));
    printf("unsigned = %d\n", sizeof(unsigned));
    printf("long = %d\n", sizeof(long));
    printf("long long = %d\n", sizeof(long long));

    for (int ii = 0; ii < 15; ++ii) {
        printf("%d\n", ii);
    }
    
    long long ll = 10000000000;
    printf("ll = %lld, ll = 0x%lld\n", ll, ll);

    //printf("Hello from C!!\r\n");

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
