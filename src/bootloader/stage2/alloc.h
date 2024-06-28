#pragma once

#include "stdtypes.h"

Bool heapInit(void* base, Uint32 size);
void* alloc(Uint32 size);
void free(void* chunk);
void printHeap();
