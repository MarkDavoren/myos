#include "stdtypes.h"
#include "stdio.h"
#include "fat.h"
#include "memdefs.h"
#include "disk.h"

void convert8D3ToString(Uint8 far* name, char* out);

#pragma pack(push, 1)
typedef struct 
{
    Uint8  BootJumpInstruction[3];
    Uint8  OemIdentifier[8];
    Uint16 BytesPerSector;
    Uint8  SectorsPerCluster;
    Uint16 ReservedSectors;
    Uint8  FatCount;
    Uint16 DirEntryCount;
    Uint16 TotalSectors;
    Uint8  MediaDescriptorType;
    Uint16 SectorsPerFat;
    Uint16 SectorsPerTrack;
    Uint16 Heads;
    Uint32 HiddenSectors;
    Uint32 LargeSectorCount;

    // extended boot record
    Uint8  DriveNumber;
    Uint8  _Reserved;
    Uint8  Signature;
    Uint32 VolumeId;
    Uint8  VolumeLabel[11];
    Uint8  SystemId[8];

    Uint8  code[448];   // To make sizeof(BootSector) == 512 bytes
    Uint16 signature;   

} BootSector;

typedef struct 
{
    Uint8  name[11];
    Uint8  attributes;
    Uint8  reserved;
    Uint8  createdTimeTenths;
    Uint16 createdTime;
    Uint16 createdDate;
    Uint16 accessedDate;
    Uint16 firstClusterHigh;
    Uint16 modifiedTime;
    Uint16 modifiedDate;
    Uint16 firstClusterLow;
    Uint32 size;
} Directory;

#pragma pack(pop)

typedef struct {
    Disk disk;
    BootSector mbr;
    Directory far* rootDirectory;
    Uint8 far* FAT;
} FatData;

static FatData far* fatData = FAT_MEM_ADDRESS;

Bool fatInitialize(Uint8 driveNumber)
{
    // Initialize disk

    if (!diskInit(&fatData->disk, driveNumber)) {
        printf("Failed to initialize disk %d\r\n", driveNumber);
        return false;
    }
    // printf("Cylinders = %d, Heads = %d, Sectors = %d\r\n",
    //     fatData->disk.numCylinders,
    //     fatData->disk.numHeads,
    //     fatData->disk.numSectors);

    // Read in MBR

    if (!diskRead(&fatData->disk, 0, 1, (Uint8 far*) &fatData->mbr)) {
        printf("Failed to read MBR of disk %d\r\n", driveNumber);
        return false;
    }

    // Read in Root Directory

    Uint16 rootDirLBA = fatData->mbr.ReservedSectors + fatData->mbr.FatCount * fatData->mbr.SectorsPerFat;
    Uint16 rootDirSizeInSectors = (fatData->mbr.DirEntryCount * sizeof(Directory) + fatData->mbr.BytesPerSector - 1) / fatData->mbr.BytesPerSector;
    Uint16 rootDirTotalSizeInBytes = rootDirSizeInSectors * fatData->mbr.BytesPerSector;

    if (sizeof(FatData) + rootDirTotalSizeInBytes > FAT_MEM_SIZE) {
        printf("Insufficent space to load root directory. Need %x. Have %x",
            rootDirTotalSizeInBytes, FAT_MEM_SIZE - (sizeof(FatData)));
    }

    fatData->rootDirectory = (Directory far*) (fatData + 1); // RootDirectory loaded immediately after FatData

    if (!diskRead(&fatData->disk, rootDirLBA, rootDirSizeInSectors, (Uint8 far*) fatData->rootDirectory)) {
        printf("Failed to read Root directory of disk %d\r\n", driveNumber);
        return false;
    }

    Directory far* dir = fatData->rootDirectory;
    for (int ii = 0; ii < fatData->mbr.DirEntryCount; ++ii, ++dir) {
        if (dir->name[0] == '\0') {
            continue;
        }

        char name[13]; // 8 + '.' + 3 + null
        convert8D3ToString(dir->name, name);

        printf("'%s' %x %d %ld\r\n", name, dir->attributes, dir->firstClusterLow, dir->size);
    }

    // Load FAT

    fatData->FAT = (Uint8 far*) (fatData->rootDirectory + fatData->mbr.DirEntryCount);
    if (!diskRead(&fatData->disk, fatData->mbr.ReservedSectors, fatData->mbr.SectorsPerFat, (Uint8 far*) fatData->FAT)) {
        printf("Failed to read FAT of disk %d\r\n", driveNumber);
        return false;
    }

    // In FAT 12, cluster numbers in the FAT are 12 bits long
    // So multiply the cluster number by 3/2 to get a word index into the FAT
    // If the cluster number is even, we want the bottom 12 bits
    // Otherwise we want the top 12 bits

    for (Uint16 index = 0; index < 16; ++index) {
        printf("%x ", fatData->FAT[index]);
    }
    printf("\r\n");

    for (Uint16 cluster = 0; cluster < 16; ++cluster) {
        Uint16 index = (cluster * 3) / 2;
        Uint16 value = *(Uint16 far*)&fatData->FAT[index];
        if ((cluster % 2) == 0) {
            value &= 0xFFF;
        } else {
            value >>= 4;
        }
        printf("%x ", value);
    }
    printf("\r\n");



    return true;
}

File far* fatOpen(const char* path);
Uint32 fatRead(File far* file, Uint32 byteCount, void far* buffer);
void fatClose(File far* file);

/*
 * Convert an 8.3 FAT filename to a string
 *
 * 8.3 filenames cannot contain spaces, so if they are there skip them as they are padding
 * The '.' and extension are optional
 * 
 * Caller is responsible for ensuring that out has space for at least 13 characters: 8 + '.' + 3 + null
 */
void convert8D3ToString(Uint8 far* name, char* out) {
    if (name[0] == '\0') {
        *out = '\0';
        return;
    }
    for (int ii = 0; ii < 8; ++ii) {
        if (name[ii] == ' ') {
            break;
        }
        *out++ = name[ii];
    }
    if (name[8] != ' ') {
        *out++ = '.';
    }
    for (int ii = 8; ii < 11; ++ii) {
        if (name[ii] == ' ') {
            break;
        }
        *out++ = name[ii];
    }
    *out = '\0';
}
