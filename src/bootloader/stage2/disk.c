#include "stdtypes.h"
#include "disk.h"
#include "x86.h"
#include "stdio.h"

/*
 *  Initialize the Disk object for the specified drive number from BIOS details
 */
bool diskInit(Disk* disk, uint8_t driveNumber)
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

/*
 * Read count sectors from disk starting at lba into buffer
 */
bool diskRead(Disk* disk, uint32_t lba, uint8_t* countPtr, uint8_t far* buffer)
{
    uint8_t requestedCount = *countPtr;
    uint16_t cylinder, head, sector;
    uint8_t status;
    bool ok;

    sector = (lba % disk->numSectors) + 1;
    cylinder = (lba / disk->numSectors) / disk->numHeads;
    head = (lba / disk->numSectors) % disk->numHeads;

    printf("Read: lba = %lu, cylinder = %u, head = %u, sector = %u, count = %d\r\n", lba, cylinder, head, sector, *countPtr);

    // Ralf Brown recommends trying up to three times to read with a reset between attempts
    // since the read may fail due the motor failing to spin up quickly enough
    // Not an issue with qemu, but is a best practice

    // On Bochs, a failed read will set *count to 0
    // On Qemu, a failed read will leave *count unchanged
    for (int retries = 0; retries < 3; retries++) {
        *countPtr = requestedCount;
        ok = bios_readDisk(disk->id, cylinder, head, sector, countPtr, buffer, &status);
        printf("OK = %x, Status = %x\r\n", ok, status);
        if (ok) {
            if (*countPtr != requestedCount) {
                printf("Successful short read %d of %d\r\n", *countPtr, requestedCount);
            }
            return true;
        }
        printf("Retrying disk read...\r\n");

        if (*countPtr != requestedCount) {
            printf("Failed short read %d of %d\r\n", *countPtr, requestedCount);
        }

        if (!bios_resetDisk(disk->id)) {
            printf("Reset disk %d failed \r\n", disk->id);
        }
    }

    return false;
}
