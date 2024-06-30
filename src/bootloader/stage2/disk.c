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
    Uint16 numCylinders, numHeads, numSectors, bytesPerSectors;

    if (!bios_getDriveParams(driveNumber, &numCylinders, &numHeads, &numSectors)) {
        return false;
    }

    if (!bios_getExtDriveParams(driveNumber, &bytesPerSectors)) {
        return false;
    }

    disk->id = driveNumber;
    disk->numCylinders = numCylinders;
    disk->numHeads = numHeads;
    disk->numSectors = numSectors;
    disk->bytesPerSector = bytesPerSectors;
    disk->offset = part->lba;

    printf("diskInit: Cylinders = %d, Heads = %d, Sectors = %d, offset = %d, bps = %d\n",
        disk->numCylinders,
        disk->numHeads,
        disk->numSectors,
        disk->offset,
        disk->bytesPerSector);
    return true;
}

/*
 * Read count sectors from disk starting at lba into buffer
 *
 * Deprecated as it's lba and count have unacceptable limits
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

    printf("Read: lba = %u, cylinder = %u, head = %u, sector = %u, count = %u\n", lba, cylinder, head, sector, count);

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
 * Extended form of diskRead
 *
 * diskRead takes a Uint8 count so anything > 255 won't work
 * Furthermore the CHS address system has an 8MB limit
 *
 * diskExtRead takes a 16 bit count and directly uses the lba
 * 
 * It calls a BIOS function that requires BIOS extensions enabled
 */
Bool diskExtRead(Disk* disk, Uint32 lba, Uint16 count, Uint8* buff)
{
    Uint8 status;

    lba += disk->offset;

    //printf("diskExtRead: lba = %#x, count = %#x sectors, buff= %#p\n", lba, count, buff);
    Bool ok = bios_ExtReadDisk(disk->id, lba, count, buff, &status);
    //printf("OK = %d, Status = %#x\n", ok, status);

    return ok;

    // Uint16 limit = count;
    // if (limit > 255) { // Max Uint8
    //     limit = 128;
    //     if (disk->bytesPerSector == 0) {
    //         panic("diskExtRead: bytes per sector == 0");
    //     }
    // }

    // while (count > 0) {
    //     if (!diskRead(disk, lba, limit, buff)) {
    //         return false;
    //     }
    //     lba += limit;
    //     buff += limit * disk->bytesPerSector;
    //     count -= limit;
    // }

    return true;
}