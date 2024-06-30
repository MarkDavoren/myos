#pragma once

#include "stdtypes.h"
#include "mbr.h"

#ifndef BAD_HANDLE
typedef Int8 Handle;
#define BAD_HANDLE -1
#endif

Bool extInitialize(Uint8 driveNumber, Partition* part);
Handle extOpen(const char*);
Uint32 extRead(Handle fin, Uint32 count, void* buff);
void extClose(Handle handle);

