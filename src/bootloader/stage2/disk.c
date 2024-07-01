#include "stdtypes.h"
#include "disk.h"
#include "bios.h"
#include "mbr.h"
#include "stdio.h"
#include "utility.h"

/*
 * Initialize the Disk object for the specified drive number from BIOS details
 *
 * If there are no partitions, it is expected that part will point to an empty partition block
 * in which case lba == 0 which is the offset we want for a non-partitioned disk
 * 
 * If disk extensions are not enabled for this drive then we guess that there are 512 bytes per sector
 * The caller should confirm/overwrite from other sources such as the MBR
 */
Bool diskInit(Disk* disk, Uint8 driveNumber, Partition* part)
{
    Uint16 numCylinders, numHeads, numSectors, bytesPerSectors;

    disk->hasExtensions = bios_hasDiskExtensions(driveNumber);
    printf("hasExtensions = %d\n", disk->hasExtensions);

    if (!bios_getDriveParams(driveNumber, &numCylinders, &numHeads, &numSectors)) {
        printf("diskInit: Cannot get drive params\n");
        return false;
    }

    if (!disk->hasExtensions || !bios_getExtDriveParams(driveNumber, &bytesPerSectors)) {
        printf("diskInit: Cannot get drive extended params. Guessing at bytes per sector\n");
        bytesPerSectors = 512;  // Default value
    }

    disk->id = driveNumber;
    disk->numCylinders = numCylinders;
    disk->numHeads = numHeads;
    disk->numSectors = numSectors;
    disk->bytesPerSector = bytesPerSectors;
    disk->offset = part->lba;

    printf("diskInit: Cylinders = %d, Heads = %d, Sectors = %d, offset = %d, bps = %d, ext? = %d\n",
        disk->numCylinders,
        disk->numHeads,
        disk->numSectors,
        disk->offset,
        disk->bytesPerSector,
        disk->hasExtensions);

    return true;
}

/*
 * Read count sectors from disk starting at lba into buffer
 *
 * Warning: count must be < 255. LBA is limited by the CHS geometry
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

    //printf("diskRead: lba = %u, cylinder = %u, head = %u, sector = %u, count = %u\n", lba, cylinder, head, sector, count);

    // Ralf Brown recommends trying up to three times to read with a reset between attempts
    // since the read may fail due the motor failing to spin up quickly enough
    // Not an issue with qemu, but is a best practice

    // On Bochs, a failed read will set *count to 0
    // On Qemu, a failed read will leave *count unchanged
    for (int retries = 0; retries < 3; retries++) {
        ok = bios_readDisk(disk->id, cylinder, head, sector, count, buffer, &status);
        //printf("OK = %x, Status = %x\n", ok, status);

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
 * Extended form of diskRead - calls bios_ExtReadDisk
 *
 * bios_ExtReadDisk takes a 16 bit count and directly uses the lba,
 *   but only works if disk extensions are enabled
 * 
 * if disk extensions are not enabled then tries calling diskRead
 *   diskRead takes a Uint8 count so anything > 255 won't work
 *   Furthermore the CHS address system has an 8MB limit
 */
Bool diskExtRead(Disk* disk, Uint32 lba, Uint16 count, Uint8* buff)
{
    Uint8 status;
    Bool ok;

    lba += disk->offset;

    //printf("diskExtRead: lba = %#x, count = %#x sectors, buff= %#p\n", lba, count, buff);

    if (disk->hasExtensions) {
        ok = bios_ExtReadDisk(disk->id, lba, count, buff, &status);
        //printf("OK = %d, Status = %#x\n", ok, status);
    } else if (count < 0x100 && lba < disk->numCylinders * disk->numHeads * disk->numSectors) {
        ok = diskRead(disk, lba, count, buff);
    } else {
        printf("Attempted read with invalid params for non-extended disk: lba = %#x, count = %#x sectors\n", lba, count);
        panic("Cannot read disk");
        ok = false; // Should never get here
    }

    return ok;
}