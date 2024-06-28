#pragma once

#include "stdtypes.h"

Bool __attribute__((cdecl)) bios_getDriveParams(
                                Uint8   driveNumber,
                                Uint16* numCylinders,
                                Uint16* numHeads,
                                Uint16* numSectors);

Bool __attribute__((cdecl)) bios_resetDisk(Uint8 driveNumber);

Bool __attribute__((cdecl)) bios_readDisk(
                                Uint8  driveNumber,
                                Uint16 cylinder,
                                Uint16 head,
                                Uint16 sector,
                                Uint8  count,
                                Uint8* buffer,
                                Uint8* status);
                
Bool __attribute__((cdecl)) bios_getExtDriveParams(
                                Uint8   driveNumber,
                                Uint16* bytesPerSectors);

Bool __attribute__((cdecl)) bios_ExtReadDisk(
                                Uint8 id,
                                Uint32 lba,
                                Uint16 count,
                                Uint8* buff,
                                Uint8* status);

