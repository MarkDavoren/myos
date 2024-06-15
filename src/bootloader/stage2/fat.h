#pragma once

#include"stdtypes.h"
#include "disk.h"

typedef Int8 Handle;
#define BAD_HANDLE -1

Bool fatInitialize(Uint8 driveNumber);
Handle fatOpen(const char* path);
Uint32 fatRead(Handle handle, Uint32 byteCount, void far* buffer);
void fatClose(Handle handle);

enum FatAttributes
{
    FAT_ATTRIBUTE_READ_ONLY         = 0x01,
    FAT_ATTRIBUTE_HIDDEN            = 0x02,
    FAT_ATTRIBUTE_SYSTEM            = 0x04,
    FAT_ATTRIBUTE_VOLUME_ID         = 0x08,
    FAT_ATTRIBUTE_DIRECTORY         = 0x10,
    FAT_ATTRIBUTE_ARCHIVE           = 0x20,
    FAT_ATTRIBUTE_LFN               = FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID
};
