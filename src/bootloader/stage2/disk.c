#include "stdtypes.h"
#include "disk.h"
#include "bios.h"
#include "mbr.h"
#include "stdio.h"
#include "utility.h"

/*
 *  Initialize the Disk object for the specified drive number from BIOS details
 */
Bool diskInit(Disk* disk, Uint8 driveNumber, Partition* part)
{
    Uint16 numCylinders, numHeads, numSectors;

    if (!bios_getDriveParams(driveNumber, &numCylinders, &numHeads, &numSectors)) {
        return false;
    }

    disk->id = driveNumber;
    disk->numCylinders = numCylinders;
    disk->numHeads = numHeads;
    disk->numSectors = numSectors;
    disk->bytesPerSector = 0;   // This can't be set until we read in the MBR -- see fat.c
    disk->offset = part->lba;

    printf("diskInit: Cylinders = %d, Heads = %d, Sectors = %d, offset = %d\n",
        disk->numCylinders,
        disk->numHeads,
        disk->numSectors,
        disk->offset);
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

    lba += disk->offset;

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

/*
 * diskRead takes a Uint8 count so anything > 255 won't work
 * If count is > 255, split the reads into 128 byte chunks plus whatever the last chunk is
 */
Bool diskBigRead(Disk* disk, Uint32 lba, Uint16 count, Uint8* buff)
{
    Uint16 limit = count;
    if (limit > 255) { // Max Uint8
        limit = 128;
        if (disk->bytesPerSector == 0) {
            panic("diskBigRead: bytes per sector == 0");
        }
    }

    while (count > 0) {
        if (!diskRead(disk, lba, limit, buff)) {
            return false;
        }
        lba += limit;
        buff += limit * disk->bytesPerSector;
        count -= limit;
    }

    return true;
}