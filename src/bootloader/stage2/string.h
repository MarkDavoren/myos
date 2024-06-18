#pragma once
#include "stdtypes.h"

void* memcpy(void* dst, const void* src, Uint16 num);
void* memset(void* ptr, int value, Uint16 num);
int memcmp(const void* ptr1, const void* ptr2, Uint16 num);

const char* strchr(const char* str, char chr);
char* strcpy(char* dst, const char* src);
unsigned strlen(const char* str);
