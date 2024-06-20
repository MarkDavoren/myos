#include "stdtypes.h"
#include "stdio.h"
#include "string.h"
#include "hal/hal.h"
#include "crashme.h"

extern Uint8 __bss_start;
extern Uint8 __bss_end;

void __attribute__((section(".entry"))) start(Uint16 bootDrive)
{
    memset(&__bss_start, 0, (&__bss_end) - (&__bss_start));

    halInitialize();

    clearScreen();

    printf("Hello from the kernel!!\n");

    //crashMeInt64h();
    //crashMeDiv0();
    crashMeInt8();
}
