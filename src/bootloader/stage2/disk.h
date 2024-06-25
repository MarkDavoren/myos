#pragma once

#include "stdtypes.h"
#include "mbr.h"

typedef struct {
    Uint8   id;
    Uint16  numCylinders;
    Uint16  numHeads;
    Uint16  numSectors;
    Uint32  offset;     // LBA offset to start of partition
} Disk;

Bool diskInit(Disk* disk, Uint8 driveNumber, Partition* part);
Bool diskRead(Disk* disk, Uint32 lba, Uint8 count, Uint8* data);
Bool diskBigRead(Disk* disk, Uint32 lba, Uint16 count, Uint8* buff);
