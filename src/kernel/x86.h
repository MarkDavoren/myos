#pragma once

#include "stdtypes.h"

void __attribute__((cdecl)) x86_outb(Uint16 port, Uint8 value);
Uint8 __attribute__((cdecl)) x86_inb(Uint16 port);
