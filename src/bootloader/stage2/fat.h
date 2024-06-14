#pragma once

#include"stdtypes.h"
#include "disk.h"

typedef File;

Bool fatInitialize(Uint8 driveNumber);
File far* fatOpen(const char* path);
Uint32 fatRead(File far* file, Uint32 byteCount, void far* buffer);
void fatClose(File far* file);
