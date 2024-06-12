#include "stdtypes.h"
#include "stdio.h"
#include "x86.h"

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

typedef enum length_t {NORMAL, SHORT_SHORT, SHORT, LONG, LONG_LONG} length_t;
const char hex[16] = "0123456789ABCDEF";

int* putnum(int* argp, length_t length, bool sign, int radix)
{
    unsigned long long number;
    bool negative = false;
    int pos = 0;
    char buf[32];

    /*
     * Assuming we are in 16 bit mode and an int and an int* is 2 bytes
     */
    /*
     * Assuming we are using two's complement and that casting back and forth
     * between signed and unsigned will not change the value
     */
    switch (length) {
        case NORMAL:        // In 16 bit mode, an int is a short
        case SHORT_SHORT:   // In 16 bit mode, you can't push just a byte onto the stack, so treat as a short
        case SHORT: {
            int n = *argp;
            if (sign && n < 0) {
                n = -n;
                negative = true;
            }
            number = (unsigned) n;
            argp++;
            break;
        }

        case LONG: {
            long int n = *(long int*)argp;
            if (sign && n < 0) {
                n = -n;
                negative = true;
            }
            number = (unsigned long long) n;
            argp += 2;
            break;
        }

        case LONG_LONG: {
            long long int n = *(long long int*)argp;
            if (sign && n < 0) {
                n = -n;
                negative = true;
            }
            number = (unsigned long long) n;
            argp += 4;
            break;
        }
    }

    do {
        uint32_t rem;
        x86_div64_32(number, radix, &number, &rem);
        buf[pos++] =  hex[rem];
    } while (number > 0);

    if (negative) {
        buf[pos++] = '-';
    }

    if (radix == 16) {
        buf[pos++] = 'x';
        buf[pos++] = '0';
    } else if (radix == 8 && buf[pos-1] != '0') {
        buf[pos++] = '0';
    }

    while (pos-- > 0) {
        putc(buf[pos]);
    }

    return argp;
}

void _cdecl printf(const char* fmt, ...)
{
    int* argp = ((int*)&fmt) + 1;
    length_t length = NORMAL;

    while (*fmt) {
        switch (*fmt) {
        case '%':
            for (;;) {
                switch (*++fmt) {
                case '%':
                    putc('%');
                    break;
                case 'c':
                    putc((char) *argp);
                    argp++;
                    break;
                case 's':
                    puts(*(const char**) argp);
                    argp++;
                    break;
                case 'd':
                case 'i':
                    argp = putnum(argp, length, true, 10);
                    break;
                case 'u':
                    argp = putnum(argp, length, false, 10);
                    break;
                case 'x':
                case 'p':
                    argp = putnum(argp, length, false, 16);
                    break;
                case 'o':
                    argp = putnum(argp, length, false, 8);
                    break;
                case 'h':
                    if (*(fmt+1) == 'h') {
                        fmt++;
                        length = SHORT_SHORT;
                    } else {
                        length = SHORT;
                    }
                    continue;
                case 'l':
                    if (*(fmt+1) == 'l') {
                        fmt++;
                        length = LONG_LONG;
                    } else {
                        length = LONG;
                    }
                    continue;
                }
                length = NORMAL;
                break;
            }
            break;

        default:
            putc(*fmt);
        }
        fmt++;
    }
}
