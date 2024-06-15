#pragma once
#include "stdtypes.h"

void far* memcpy(void far* dst, const void far* src, Uint16 num);
void far* memset(void far* ptr, int value, Uint16 num);
int memcmp(const void far* ptr1, const void far* ptr2, Uint16 num);

const char* strchr(const char* str, char chr);
char* strcpy(char* dst, const char* src);
unsigned strlen(const char* str);
