#include "string.h"
#include "stdtypes.h"

void* memcpy(void* dst, const void* src, Uint16 num)
{
    Uint8* u8Dst       = (Uint8 *) dst;
    const Uint8* u8Src = (const Uint8 *) src;

    for (Uint16 ii = 0; ii < num; ++ii) {
        u8Dst[ii] = u8Src[ii];
    }

    return dst;
}

void * memset(void * ptr, int value, Uint16 num)
{
    Uint8* u8Ptr = (Uint8 *)ptr;

    for (Uint16 ii = 0; ii < num; ++ii) {
        u8Ptr[ii] = (Uint8) value;      // In real C, this is what happens. Passed as int, but used as unsigned char
    }

    return ptr;
}

int memcmp(const void* ptr1, const void* ptr2, Uint16 num)
{
    const Uint8* u8Ptr1 = (const Uint8 *)ptr1;
    const Uint8* u8Ptr2 = (const Uint8 *)ptr2;

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

const char* getComponent(const char* path, char* component, char delimiter, int limit)
{
    --limit; // Leave room for null

    for (int ii = 0; *path != '\0' && *path != delimiter && ii < limit; ii++) {
        *component++ = *path++;
    }
    *component = '\0';

    return path;
}

