#include "stdtypes.h"
#include "stdio.h"
#include "x86.h"

const unsigned SCREEN_WIDTH = 80;
const unsigned SCREEN_HEIGHT = 24;
const unsigned DEFAULT_COLOR = 0x7;

Uint8* screen = (Uint8*) 0xB8000;
int currentX = 0, currentY = 0;

void putChar(int x, int y, char c)
{
    screen[2 * (y * SCREEN_WIDTH + x)] = c;
}

void putColor(int x, int y, Uint8 color)
{
        screen[2 * (y * SCREEN_WIDTH + x) + 1] = color;
}

void setCursor(int x, int y)
{
    // TODO
}

void clearScreen()
{
    for (int yy = 0; yy < SCREEN_HEIGHT; ++yy)
        for (int xx = 0; xx < SCREEN_WIDTH; ++xx) {
            putChar(xx, yy, '\0');
            putColor(xx, yy, DEFAULT_COLOR);
    }
    currentX = 0;
    currentY = 0;
    setCursor(0, 0);
}

void scroll(int numLines)
{
    // TODO
}

void putc(char c)
{
    switch (c) {
    case '\n':
        currentY++;
        break;
    
    case '\r':
        currentX = 0;
        break;
    
    case '\t':
        for (int ii = 0; ii < 4 - (currentX % 4); ++ii) {
            putc(' ');
        }
        break;
    
    default:
        putChar(currentX, currentY, c);
        currentX++;
        break;
    }

    if (currentX >= SCREEN_WIDTH) {
        currentX = 0;
        currentY++;
    }

    if (currentY >= SCREEN_HEIGHT) {
        scroll(1);
    }

    setCursor(currentX, currentY);
}

void puts(const char* str)
{
    while (*str) {
        putc(*str++);
    }
}

typedef enum length_t {NORMAL, SHORT_SHORT, SHORT, LONG, LONG_LONG} length_t;
const char hex[16] = "0123456789ABCDEF";

int* putnum(int* argp, length_t length, Bool sign, int radix)
{
    unsigned long long number;
    Bool negative = false;
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
        Uint32 rem;
        x86_div64_32(number, radix, &number, &rem);
        buf[pos++] =  hex[rem];
    } while (number > 0);

    if (negative) {
        buf[pos++] = '-';
    }

    // if (radix == 16) {
    //     buf[pos++] = 'x';
    //     buf[pos++] = '0';
    // } else if (radix == 8 && buf[pos-1] != '0') {
    //     buf[pos++] = '0';
    // }

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
