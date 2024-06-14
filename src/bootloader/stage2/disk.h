#pragma once

#include "stdtypes.h"

typedef struct {
    Uint8     id;
    Uint16    numCylinders;
    Uint16    numHeads;
    Uint16    numSectors;
} Disk;

Bool diskInit(Disk far* disk, Uint8 driveNumber);
Bool diskRead(Disk far* disk, Uint32 lba, Uint8 count, Uint8 far* data);
