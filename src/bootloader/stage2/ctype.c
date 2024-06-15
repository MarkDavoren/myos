#include "ctype.h"

Bool isLower(char chr)
{
    return chr >= 'a' && chr <= 'z';
}

char toUpper(char chr)
{
    return isLower(chr) ? (chr - 'a' + 'A') : chr;
}
