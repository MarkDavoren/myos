#include "stdtypes.h"
#include "mbr.h"
#include "stdio.h"

void printPartitionTable(Partition* pp)
{
    for (int ii = 0; ii < 4; ++ii) {
        printf("%d: attr=%x, startCHS=[%x,%x,%x], type=%x, lastCHS=[%x,%x,%x], lba=%x, size=%x\n",
            ii, pp->attributes,
            pp->startCHS[0], pp->startCHS[1], pp->startCHS[2],
            pp->type,
            pp->lastCHS[0], pp->lastCHS[1], pp->lastCHS[2],
            pp->lba, pp->size);
            ++pp;
    }
}