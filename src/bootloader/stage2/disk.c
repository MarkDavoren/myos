#include "stdtypes.h"
#include "disk.h"
#include "bios.h"
#include "stdio.h"

/*
 *  Initialize the Disk object for the specified drive number from BIOS details
 */
Bool diskInit(Disk* disk, Uint8 driveNumber)
{
    Uint16 numCylinders, numHeads, numSectors;

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
Bool diskRead(Disk* disk, Uint32 lba, Uint8 count, Uint8* buffer)
{
    Uint16 cylinder, head, sector;
    Uint8 status;
    Bool ok;

    sector = (lba % disk->numSectors) + 1;
    cylinder = (lba / disk->numSectors) / disk->numHeads;
    head = (lba / disk->numSectors) % disk->numHeads;

    printf("Read: lba = %lu, cylinder = %u, head = %u, sector = %u, count = %d\n", lba, cylinder, head, sector, count);

    // Ralf Brown recommends trying up to three times to read with a reset between attempts
    // since the read may fail due the motor failing to spin up quickly enough
    // Not an issue with qemu, but is a best practice

    // On Bochs, a failed read will set *count to 0
    // On Qemu, a failed read will leave *count unchanged
    for (int retries = 0; retries < 3; retries++) {
        ok = bios_readDisk(disk->id, cylinder, head, sector, count, buffer, &status);
        printf("OK = %x, Status = %x\n", ok, status);
        if (ok) {
            return true;
        }
        printf("Retrying disk read...\n");

        if (!bios_resetDisk(disk->id)) {
            printf("Reset disk %d failed \n", disk->id);
        }
    }

    return false;
}
