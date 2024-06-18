#pragma once

void putc(char c);
void puts(const char* str);
void __attribute__((cdecl)) printf(const char* fmt, ...);
void clearScreen();
