#pragma once

#include "stdtypes.h"
#include "ext.h"

typedef enum {
    FAT = 0,
    EXT = 1
} FilesystemType;

void    vSetType(FilesystemType);
Bool    vInitialize(Uint8 driveNumber, Partition* part);
Handle  vOpen(const char* path);
Uint32  vRead(Handle fin, Uint32 count, void* buff);
void    vClose(Handle handle);
