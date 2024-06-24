#pragma once

#include "stdtypes.h"

typedef struct {
    Uint8   attributes;     // Bit 7 means partition is bootable
    Uint8   startCHS[3];    // CHS of first sector in partition
    Uint8   type;           // Type of partition
    Uint8   lastCHS[3];     // CHS address of last sector in partition
    Uint32  lba;            // LBA of start of partition
    Uint32  size;           // In sectors
} __attribute__((packed)) Partition;

void printPartitionTable(Partition* pp);
