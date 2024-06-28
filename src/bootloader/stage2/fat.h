#pragma once

#include"stdtypes.h"
#include "disk.h"
#include "mbr.h"

#ifndef BAD_HANDLE
typedef Int8 Handle;
#define BAD_HANDLE -1
#endif

Bool fatInitialize(Uint8 driveNumber, Partition* part);
Handle fatOpen(const char* path);
Uint32 fatRead(Handle handle, Uint32 byteCount, void* buffer);
void fatClose(Handle handle);
