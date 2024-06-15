#include "utility.h"
#include "stdio.h"

Uint32 align(Uint32 number, Uint32 alignTo)
{
    if (alignTo == 0)
        return number;

    Uint32 rem = number % alignTo;
    return (rem > 0) ? (number + alignTo - rem) : number;
}
