#pragma once
#include "stdtypes.h"

Uint32 roundup(Uint32 number, Uint32 size);
Uint32 align(Uint32 number, Uint32 alignTo);
void panic(char* msg);