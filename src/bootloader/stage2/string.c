#include "string.h"
#include "stdtypes.h"

void far* memcpy(void far* dst, const void far* src, Uint16 num)
{
    Uint8 far* u8Dst       = (Uint8 far *) dst;
    const Uint8 far* u8Src = (const Uint8 far *) src;

    for (Uint16 ii = 0; ii < num; ++ii) {
        u8Dst[ii] = u8Src[ii];
    }

    return dst;
}

void far * memset(void far * ptr, int value, Uint16 num)
{
    Uint8 far* u8Ptr = (Uint8 far *)ptr;

    for (Uint16 ii = 0; ii < num; ++ii) {
        u8Ptr[ii] = (Uint8) value;      // In real C, this is what happens. Passed as int, but used as unsigned char
    }

    return ptr;
}

int memcmp(const void far* ptr1, const void far* ptr2, Uint16 num)
{
    const Uint8 far* u8Ptr1 = (const Uint8 far *)ptr1;
    const Uint8 far* u8Ptr2 = (const Uint8 far *)ptr2;

    for (Uint16 ii = 0; ii < num; ++ii)
        if (u8Ptr1[ii] != u8Ptr2[ii])
            return 1;

    return 0;
}

const char* strchr(const char* str, char chr)
{
    if (str == NULL) {
        return NULL;
    }

    for (; *str; ++str) {
        if (*str == chr)
            return str;
    }

    return NULL;
}

char* strcpy(char* dst, const char* src)
{
    char* origDst = dst;

    if (dst == NULL) {
        return NULL;
    }

    if (src == NULL) {
        *dst = '\0';
        return dst;
    }

    while (*src) {
        *dst++ = *src++;
    }
    
    *dst = '\0';
    return origDst;
}

unsigned strlen(const char* str)
{
    unsigned len = 0;
    while (*str)
    {
        ++len;
        ++str;
    }

    return len;
}
