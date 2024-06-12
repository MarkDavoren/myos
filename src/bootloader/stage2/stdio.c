#include "stdio.h"
#include "bios.h"

void putc(char c)
{
    bios_putc(c, 0);
}

void puts(const char* str)
{
    while (*str) {
        putc(*str++);
    }
}