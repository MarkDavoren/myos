#include "utility.h"
#include "stdio.h"

Uint32 roundup(Uint32 number, Uint32 size)
{
    return (number + size - 1) / size;
}

Uint32 align(Uint32 number, Uint32 alignTo)
{
    if (alignTo == 0)
        return number;

    Uint32 rem = number % alignTo;
    return (rem > 0) ? (number + alignTo - rem) : number;
}

void panic(char* msg)
{
    printf("Bootloader panic: %s\n", msg);
    for (;;) ;
}
