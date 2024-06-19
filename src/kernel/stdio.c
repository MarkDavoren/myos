#include <stdarg.h>
#include "stdtypes.h"
#include "stdio.h"
#include "arch/i686/io.h"

const unsigned SCREEN_WIDTH = 80;       // As defined by VGA
const unsigned SCREEN_HEIGHT = 25;      // As defined by VGA
const unsigned DEFAULT_COLOR = 0x0F;    // White (F) on black (0)

Uint8* screen = (Uint8*) 0xB8000;
int currentX = 0, currentY = 0;

void putChar(int x, int y, char c)
{
    screen[2 * (y * SCREEN_WIDTH + x)] = c;
}

char getChar(int x, int y)
{
    return screen[2 * (y * SCREEN_WIDTH + x)];
}

void putColor(int x, int y, Uint8 color)
{
        screen[2 * (y * SCREEN_WIDTH + x) + 1] = color;
}

Uint8 getColor(int x, int y)
{
    return screen[2 * (y * SCREEN_WIDTH + x) + 1];
}

void setCursor(int x, int y)
{
    int pos = y * SCREEN_WIDTH + x;

    /*
     * The VGA controller has many registers which are accessed via ports
     * This requires the x86 out instruction, not writing to some memory location
     * Hence the drop into assembly language
     * 
     * 0x3D4 is a port where you output an index value to select the target register
     * and then you output the value to 0x3D5
     * 
     * Target register 0x0F holds the top 8 bits of the cursor location
     * Target register 0x0E holds the bottom 8 bits
     * where the location is Y * Screen Width + X
     */
    i686_outb(0x3D4, 0x0F);                  // Cursor location high register - top 8 bits
    i686_outb(0x3D5, (Uint8)(pos & 0xFF));
    i686_outb(0x3D4, 0x0E);                  // Cursor location low register - bottom 8 bits
    i686_outb(0x3D5, (Uint8)((pos >> 8) & 0xFF));    
}

void clearScreen()
{
    for (int yy = 0; yy < SCREEN_HEIGHT; ++yy) {
        for (int xx = 0; xx < SCREEN_WIDTH; ++xx) {
            putChar(xx, yy, '\0');
            putColor(xx, yy, DEFAULT_COLOR);
        }
    }
    currentX = 0;
    currentY = 0;
    setCursor(0, 0);
}

void scroll(int numLines)
{
    for (int yy = numLines; yy < SCREEN_HEIGHT; ++yy) {
        for (int xx = 0; xx < SCREEN_WIDTH; ++xx) {
            putChar(xx, yy - numLines, getChar(xx, yy));
            putColor(xx, yy - numLines, getColor(xx, yy));
        }
    }

    for (int yy = SCREEN_HEIGHT - numLines; yy < SCREEN_HEIGHT; ++yy) {
        for (int xx = 0; xx < SCREEN_WIDTH; ++xx) {
            putChar(xx, yy, '\0');
            putColor(xx, yy, DEFAULT_COLOR);
        }
    }

    currentY -= numLines;
}

void putc(char c)
{
    switch (c) {
    case '\n':
        currentX = 0;
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

typedef enum Length {NORMAL, SHORT_SHORT, SHORT, LONG, LONG_LONG} Length;
const char hex[16] = "0123456789ABCDEF";

void printUnsigned(unsigned long long number, int radix)
{
    int pos = 0;
    char buf[32];

    do {
        Uint32 rem;
        rem = number % radix;
        number = number / radix;
        buf[pos++] =  hex[rem];
    } while (number > 0);

    // if (radix == 16) {
    //     buf[pos++] = 'x';
    //     buf[pos++] = '0';
    // } else if (radix == 8 && buf[pos-1] != '0') {
    //     buf[pos++] = '0';
    // }

    while (pos-- > 0) {
        putc(buf[pos]);
    }
}

void printSigned(long long number, int radix)
{
    if (number < 0) {
        putc('-');
        number = -number;
    }

    printUnsigned(number, radix);
}

void printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    Length length = NORMAL;

    while (*fmt) {
        switch (*fmt) {
        case '%':
            for (;;) {
                switch (*++fmt) {
                case '%':
                    putc('%');
                    break;
                case 'c':
                    putc(va_arg(args, int)); // Must be an int not a char
                    break;
                case 's':
                    puts(va_arg(args, const char*));
                    break;
                case 'd':
                case 'i':
                case 'u':
                case 'X':
                case 'x':
                case 'p':
                case 'o':
                    Bool sign;
                    int radix;
                    switch (*fmt) {
                    case 'd':   sign =  true; radix = 10; break;
                    case 'i':   sign =  true; radix = 10; break;
                    case 'u':   sign = false; radix = 10; break;
                    case 'X':   sign = false; radix = 16; break;
                    case 'x':   sign = false; radix = 16; break;
                    case 'p':   sign = false; radix = 16; break; // In 32 bit mode sizeof(int) == sizeof(int*)
                    case 'o':   sign = false; radix =  8; break;
                    }
                    if (sign) {
                        switch (length) {
                        case SHORT_SHORT:
                        case SHORT:
                        case NORMAL:
                            printSigned(va_arg(args, int), radix);
                            break;

                        case LONG:
                            printSigned(va_arg(args, long), radix);
                            break;
                        
                        case LONG_LONG:
                            printSigned(va_arg(args, long long), radix);
                            break;
                        }
                    } else {
                        switch (length) {
                        case SHORT_SHORT:
                        case SHORT:
                        case NORMAL:
                            printUnsigned(va_arg(args, unsigned int), radix);
                            break;

                        case LONG:
                            printUnsigned(va_arg(args, unsigned long), radix);
                            break;
                        
                        case LONG_LONG:
                            printUnsigned(va_arg(args, unsigned long long), radix);
                            break;
                        }
                    }
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

    va_end(args);
}
