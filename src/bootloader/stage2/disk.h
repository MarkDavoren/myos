#pragma once

#include "stdtypes.h"

typedef struct {
    uint8_t     id;
    uint16_t    numCylinders;
    uint16_t    numHeads;
    uint16_t    numSectors;
} Disk;

bool diskInit(Disk* disk, uint8_t driveNumber);
bool diskRead(Disk* disk, uint32_t lba, uint8_t* count, uint8_t far* data);
