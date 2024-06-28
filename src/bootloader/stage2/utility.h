#pragma once
#include "stdtypes.h"

Uint32 divAndRoundUp(Uint32 number, Uint32 size);
Uint32 align(Uint32 number, Uint32 alignTo);
void panic(char* msg);
void breakpoint();