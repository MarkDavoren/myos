#pragma once

#include"stdtypes.h"
#include "disk.h"

typedef Int8 Handle;
#define BAD_HANDLE -1

Bool fatInitialize(Uint8 driveNumber);
Handle fatOpen(const char* path);
Uint32 fatRead(Handle file, Uint32 byteCount, void far* buffer);
void fatClose(Handle file);
