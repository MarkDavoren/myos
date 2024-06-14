#pragma once

#include "stdtypes.h"

typedef struct {
    Uint8     id;
    Uint16    numCylinders;
    Uint16    numHeads;
    Uint16    numSectors;
} Disk;

Bool diskInit(Disk* disk, Uint8 driveNumber);
Bool diskRead(Disk* disk, Uint32 lba, Uint8* count, Uint8 far* data);
