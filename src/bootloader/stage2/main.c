#include "stdtypes.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive)
{
    int i = 12345;
    int ni = -1;
    int hex = 0x321;
    int oct = 0123;
    long l = 70000;
    long long ll = 10000000000;

    printf("Hello %% %c %s %d %d %u %x %p %o\r\n", 'x', "Boo", i, ni, ni, hex, &hex, oct);
    printf("> %ld %lld %d %s\r\n", l, ll, i, "Yah");
    for (;;)
        ;
}
