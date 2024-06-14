#pragma once

#include "stdtypes.h"

void _cdecl bios_putc(char c, uint8_t page);
bool _cdecl bios_getDriveParams(
                uint8_t driveNumber,
                uint16_t* numCylinders,
                uint16_t* numHeads,
                uint16_t* numSectors);
bool _cdecl bios_readDisk(
                uint8_t driveNumber,
                uint16_t cylinder,
                uint16_t head,
                uint16_t sector,
                uint8_t* count,
                uint8_t far* buffer,
                uint8_t* status);
bool _cdecl bios_resetDisk(uint8_t driveNumber);

void _cdecl x86_div64_32(
                uint64_t dividend,
                uint32_t divisor,
                uint64_t* quotient,
                uint32_t* remainder);
