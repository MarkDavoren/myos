#pragma once

#include "stdtypes.h"

void _cdecl bios_putc(char c, Uint8 page);

Bool _cdecl bios_getDriveParams(
                Uint8   driveNumber,
                Uint16* numCylinders,
                Uint16* numHeads,
                Uint16* numSectors);

Bool _cdecl bios_readDisk(
                Uint8  driveNumber,
                Uint16 cylinder,
                Uint16 head,
                Uint16 sector,
                Uint8  count,
                Uint8 far* buffer,
                Uint8* status);
                
Bool _cdecl bios_resetDisk(Uint8 driveNumber);

void _cdecl x86_div64_32(
                Uint64  dividend,
                Uint32  divisor,
                Uint64* quotient,
                Uint32* remainder);
