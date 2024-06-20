#pragma once
#include "stdtypes.h"

void __attribute__((cdecl)) i686_outb(Uint16 port, Uint8 value);
Uint8 __attribute__((cdecl)) i686_inb(Uint16 port);

void __attribute__((cdecl)) i686_disableInterrupts();
void __attribute__((cdecl)) i686_enableInterrupts();

void __attribute__((cdecl)) i686_halt();


void i686_iowait();
