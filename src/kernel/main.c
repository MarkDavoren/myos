#include "stdtypes.h"
#include "stdio.h"
#include "string.h"

extern Uint8 __bss_start;
extern Uint8 __bss_end;

void __attribute__((section(".entry"))) start(Uint16 bootDrive)
{
    memset(&__bss_start, 0, (&__bss_end) - (&__bss_start));

    clearScreen();

    printf("Hello from the kernel!!\n");
    for (;;)
        ;
}
