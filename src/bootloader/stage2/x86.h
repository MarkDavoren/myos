#pragma once

#include "stdtypes.h"

void __attribute__((cdecl)) x86_outb(Uint16 port, Uint8 value);
Uint8 __attribute__((cdecl)) x86_inb(Uint16 port);

Bool __attribute__((cdecl)) bios_getDriveParams(
                                Uint8   driveNumber,
                                Uint16* numCylinders,
                                Uint16* numHeads,
                                Uint16* numSectors);

Bool __attribute__((cdecl)) bios_readDisk(
                                Uint8  driveNumber,
                                Uint16 cylinder,
                                Uint16 head,
                                Uint16 sector,
                                Uint8  count,
                                Uint8* buffer,
                                Uint8* status);
                
Bool __attribute__((cdecl)) bios_resetDisk(Uint8 driveNumber);