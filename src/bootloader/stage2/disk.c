#include "stdtypes.h"
#include "disk.h"
#include "x86.h"
#include "stdio.h"

bool diskOpen(Disk* disk, uint8_t driveNumber)
{
    uint16_t numCylinders, numHeads, numSectors;

    if (!bios_getDriveParams(driveNumber, &numCylinders, &numHeads, &numSectors)) {
        return false;
    }

    disk->id = driveNumber;
    disk->numCylinders = numCylinders;
    disk->numHeads = numHeads;
    disk->numSectors = numSectors;

    return true;

}

bool diskRead(Disk* disk, uint32_t lba, uint8_t* count, uint8_t far* buffer)
{
    uint8_t requestedCount = *count;
    uint16_t cylinder, head, sector;

    sector = (lba % disk->numSectors) + 1;
    cylinder = (lba / disk->numSectors) / disk->numHeads;
    head = (lba / disk->numSectors) % disk->numHeads;

    printf("Read: lba = %lu, cylinder = %u, head = %u, sector = %u\r\n", lba, cylinder, head, sector);

    for (int retries = 0; retries < 3; retries++) {
        *count = requestedCount;
        if (bios_readDisk(disk->id, cylinder, head, sector, count, buffer)) {
            if (*count != requestedCount) {
                printf("Successful short read %d of %d\r\n", *count, requestedCount);
            }
            return true;
        }

        if (*count != requestedCount) {
            printf("Failed short read %d of %d\r\n", *count, requestedCount);
        }

        if (!bios_resetDisk(disk->id)) {
            printf("Reset disk %d failed \r\n", disk->id);
        }
    }

    return false;
}
