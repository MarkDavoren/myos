#include "stdtypes.h"
#include "stdio.h"
#include "fat.h"
#include "memdefs.h"
#include "disk.h"
#include "ctype.h"
#include "string.h"
#include "utility.h"

#define MAX_HANDLES 10

typedef struct 
{
    Uint8  bootJumpInstruction[3];
    Uint8  oemIdentifier[8];
    Uint16 bytesPerSector;
    Uint8  sectorsPerCluster;
    Uint16 reservedSectors;
    Uint8  FatCount;
    Uint16 dirEntriesCount;
    Uint16 totalSectors;
    Uint8  mediaDescriptorType;
    Uint16 sectorsPerFat;
    Uint16 sectorsPerTrack;
    Uint16 heads;
    Uint32 hiddenSectors;
    Uint32 largeSectorCount;
} __attribute__((packed)) BiosParameterBlock;

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
} __attribute__((packed)) DirectoryEntry;

typedef struct {
    Uint8       id;                 // Handle
    Bool        isOpened;           // If false then available for use
    Bool        isRootDir;          // Flag for special handling to read the root directory
    Bool        isDir;              // True if a directory
    Uint32      firstCluster;       // First cluster
    Uint32      cluster;            // Current cluster
    Uint8       sectorInCluster;    // current sector within the current cluster
    Uint32      remaining;          // Remaining bytes in sector
    Uint32      position;           // Current position in bytes
    Uint32      size;               // Maximum position in bytes
    Uint8*  sector;                 // Buffer to hold current sector
} File;

typedef struct {
    Disk        disk;               // Disk geometry details
    BiosParameterBlock  bpb;        // BIOS Parameter Block
    Uint8*      FAT;                // Address of File Allocation Table
    Uint32      rootDirLBA;         // LBA of root directory
    Uint32      clusterBaseLBA;     // LBA of cluster table
    Uint8*      dataSectors;        // Address of storage holding a sector for each files entry
    File        files[MAX_HANDLES]; // Array of Files. One for each possible handle value
} FatData;

static FatData* fat = (FatData*) FAT_MEM_ADDRESS;

/*
 * Forward declarations
 */

File* openRootDir();
File* openFile(DirectoryEntry* entry);
Uint32 readFile(File* file, Uint32 count, Uint8* buff);
void closeFile(File* file);
Handle getFreeHandle();
const char* getComponent(const char* path, char* component, int limit);
Bool findFileInDirectory(const char* name, File* dir, DirectoryEntry* foundEntry);
Bool readDirEntry(File* dir, DirectoryEntry* entry);
Bool readNextSector(File* dir);
Bool readNextSectorFromRootDir(File* dir);
Bool readNextSectorFromFile(File* file);
Uint16 getNextClusterNumber(Uint16 current);
Uint32 clusterToLBA(Uint32 cluster);
void convert8D3ToString(const char* name, char* out);
void convertStringTo8D3(const char* name, char* out);

void printDirSector(File* dir);
void printFAT();
void printDirectoryEntry(DirectoryEntry* entry);
void printFatName(char* fatName);
void printFile(File* file);

/*
 * Public functions
 */

Bool fatInitialize(Uint8 driveNumber)
{
    // Initialize disk

    if (!diskInit(&fat->disk, driveNumber)) {
        printf("Failed to initialize disk %d\n", driveNumber);
        return false;
    }
    // printf("Cylinders = %d, Heads = %d, Sectors = %d\n",
    //     fat->disk.numCylinders,
    //     fat->disk.numHeads,
    //     fat->disk.numSectors);

    // Read MBR into a temporary location and copy BPB from there to where we want it
    Uint8* tmp = (Uint8*) (FAT_MEM_ADDRESS + sizeof(FatData));
    if (!diskRead(&fat->disk, 0, 1, tmp)) {
        printf("Failed to read MBR of disk %d\n", driveNumber);
        return false;
    }
    fat->bpb = *(BiosParameterBlock*) tmp;

    // Load FAT
    // Load the entire first FAT. TODO: This is not scalable, but will be fine for 1.44MB floppy

    fat->FAT = (Uint8*) (FAT_MEM_ADDRESS + sizeof(FatData));   // Hold FAT in mem right after fat
    if (!diskRead(
            &fat->disk,
            fat->bpb.reservedSectors,   // Start of FAT on disk
            fat->bpb.sectorsPerFat,     // Read the entire first FAT. Ignore the others as they are copies
            (Uint8*) fat->FAT)) {
        printf("Failed to read FAT of disk %d\n", driveNumber);
        return false;
    }

    fat->dataSectors = (Uint8*) FAT_MEM_ADDRESS;
    Uint32 offset = align((Uint32)(sizeof(FatData) + fat->bpb.sectorsPerFat * fat->bpb.bytesPerSector),
                          (Uint32)fat->bpb.bytesPerSector);
    fat->dataSectors += offset;

    // Save LBA of root directory and entries/sector
    fat->rootDirLBA = fat->bpb.reservedSectors + fat->bpb.FatCount * fat->bpb.sectorsPerFat;

    Uint32 rootDirSizeInBytes = fat->bpb.dirEntriesCount * sizeof(DirectoryEntry);
    Uint32 rootDirSizeInSectors = (rootDirSizeInBytes + fat->bpb.bytesPerSector - 1) / fat->bpb.bytesPerSector;
    fat->clusterBaseLBA = fat->rootDirLBA + rootDirSizeInSectors;

    // Initialize Files
    for (int ii = 0; ii < MAX_HANDLES; ++ii) {
        fat->files[ii].isOpened = false;
    }

    return true;
}

Handle fatOpen(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return BAD_HANDLE;
    }

    if (path[0] != '/') {
        printf("Failed to open file '%s': Path does not begin with a '/'\n", path);
        return BAD_HANDLE;
    }

    const char* originalPath = path;
    File* file = openRootDir();

    path++; // Skip root '/'

    while (*path != '\0') {
        char component[13]; // 8 + '.' + 3 + null
        path = getComponent(path, component, sizeof(component));
        if (*path != '\0' && *path != '/') {
            printf("Failed to open file '%s': Component '%s' is too long\n", originalPath, component);
            closeFile(file);
            return BAD_HANDLE;
        }

        DirectoryEntry entry;
        if (!findFileInDirectory(component, file, &entry)) {
            printf("Failed to open file '%s': Could not find '%s'\n", originalPath, component);
            closeFile(file);
            return BAD_HANDLE;
        }

        printf("fatOpen: ");
        printDirectoryEntry(&entry);

        closeFile(file);                     // close parent directory
        file = openFile(&entry);  // open child (file or dir)
        printf("FO: ");
        printFile(file);

        if (*path == '\0') {
            break;
        }

        path++;     // skip '/'
        if (!file->isDir) {
            printf("Failed to open file '%s': Component '%s' is not a directory\n", originalPath, component);
            closeFile(file);
        }
    }
    printf("fatOpen returning id=%d\n", file->id);
    return file->id;
}

Uint32 fatRead(Handle handle, Uint32 count, void* buff)
{
    return readFile(&fat->files[handle], count, buff);
}

void fatClose(Handle handle)
{
    closeFile(&fat->files[handle]);
}

/*
 * Private functions
 */

File* openRootDir()
{
    int handle = getFreeHandle();

    File* dir = &fat->files[handle];
    dir->id = handle;
    dir->isOpened = true;
    dir->isRootDir = true;
    dir->isDir = true;
    dir->firstCluster = 0;                      // N/A for the root dir
    dir->cluster = 0;                           // N/A for the root dir
    dir->sectorInCluster = 0;                   // N/A for the root dir
    dir->remaining = 0;                         // This will cause first read to load first sector
    dir->position = 0;                          // For directories, position is current entry#
    dir->size = fat->bpb.dirEntriesCount * fat->bpb.bytesPerSector;
    dir->sector = &fat->dataSectors[handle * fat->bpb.bytesPerSector]; // Set up space, read will be on demand

    return dir;
}

File* openFile(DirectoryEntry* entry)
{
    Handle handle = getFreeHandle();
    if (handle == BAD_HANDLE) {
        return NULL;
    }

    File* file = &fat->files[handle];

    file->id = handle;
    file->isOpened = true;
    file->isRootDir = false;
    file->isDir = (entry->attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    file->firstCluster = entry->firstClusterLow + (((Uint32)entry->firstClusterHigh) << 16);
    file->cluster = 0;                          // Cause first read to read from firstCluster
    file->sectorInCluster = 0;
    file->remaining = 0;                        // Will get reset upon first read
    file->position = 0;
    file->size = entry->size;
    file->sector = &fat->dataSectors[handle * fat->bpb.bytesPerSector]; // Set up space, read will be on demand

    printf("Opened handle %d\n", handle);
    return file;
}

Uint32 readFile(File* file, Uint32 count, Uint8* buff)
{
    printf("Requesting to read %ld bytes from file %d\n", count, file->id);
    printFile(file);
    
    Uint32 remaining = file->size - file->position;    // remaining bytes in whole file
    if (!file->isDir && remaining == 0) {
        printf("Reached EOF\n");
        return 0;
    }

    if (count > remaining) {
        count = remaining;
        printf("Reduced count to %d\n", count);
    }

    Uint32 totalRead = 0;
    while (count > 0) {
        if (file->remaining == 0) {                     // remaining bytes in sector
            if(!readNextSector(file)) {
                // This can happen when reading from subdirectories since size == 0
                return totalRead;
            }
        }

        Uint32 actual = count;
        if (file->remaining < count) {
            actual = file->remaining;
        }

        Uint16 offset = fat->bpb.bytesPerSector - file->remaining;
        printf("Copying %ld bytes, from %lx to %lx\n", actual, file->sector + offset, buff);
        memcpy(buff, file->sector + offset, actual);

        file->position  += actual;
        file->remaining -= actual;
        totalRead += actual;
        count -= actual;
    }

    printf("Total read = %ld\n", totalRead);
    return totalRead;
}

void closeFile(File* file)
{
    file->isOpened = false;
}

Handle getFreeHandle()
{
    Handle handle;
    for (handle = 0; handle < MAX_HANDLES; ++handle) {
        if(!fat->files[handle].isOpened) {
            break;
        }
    }

    if (handle == MAX_HANDLES) {
        printf("Ran out of file handles\n");
        return BAD_HANDLE;
    }

    return handle;
}

const char* getComponent(const char* path, char* component, int limit)
{
    --limit; // Leave room for null

    for (int ii = 0; *path != '\0' && *path != '/' && ii < limit; ii++) {
        *component++ = *path++;
    }
    *component = '\0';

    return path;
}

/*
 * Assumes dir is newly opened and hence position == 0
 */
Bool findFileInDirectory(const char* name, File* dir, DirectoryEntry* foundEntry)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    char fatName[11];
    convertStringTo8D3(name, fatName);

    DirectoryEntry entry;
    while (readDirEntry(dir, &entry)) {
        if (entry.name[0] == '\0') {
            continue;
        }
        printFile(dir);
        printf("Comparing ");
        printFatName(entry.name);
        printf(" to ");
        printFatName(fatName);
        printf("\n");
        if (memcmp(entry.name, fatName, 11) == 0) {
            *foundEntry = entry;
            return true;
        }
    }

    return false;
}

Bool readDirEntry(File* dir, DirectoryEntry* entry)
{
    //printf("RDE: ");
    //printFile(dir);

    if (dir->remaining == 0) {                      // remaining bytes in sector
        if(!readNextSector(dir)) {
            return false;
        }
    }

    *entry = *((DirectoryEntry*) (dir->sector + (dir->position % fat->bpb.bytesPerSector)));
    dir->position  += sizeof(DirectoryEntry);
    dir->remaining -= sizeof(DirectoryEntry);

    //printDirectoryEntry(entry);

    return true;
}

Bool readNextSector(File* dir)
{
    if (dir->isRootDir) {
        return readNextSectorFromRootDir(dir);
    } else {
        return readNextSectorFromFile(dir);
    }
}

Bool readNextSectorFromRootDir(File* dir)
{
    Uint16 sector = dir->position / fat->bpb.bytesPerSector;

    if (!diskRead(&fat->disk,
                  fat->rootDirLBA + sector,
                  1,
                  dir->sector)) {
        printf("Failed to read Root directory, sector %d\n", sector);
        return false;
    }

    dir->remaining = fat->bpb.bytesPerSector;

    return true;
}

Bool readNextSectorFromFile(File* file)
{
    Uint32 nextCluster;

    // TODO: Untested case where sectors per cluster is > 1. It never happens in FAT12
    if (file->cluster == 0) {
        nextCluster = file->firstCluster;
        file->sectorInCluster = 0;
    } else if (file->sectorInCluster < fat->bpb.sectorsPerCluster - 1) {
        nextCluster = file->cluster;
        file->sectorInCluster++;
    } else {
        nextCluster = getNextClusterNumber(file->cluster);
        file->sectorInCluster = 0;
    }
    printf("Current cluster = 0x%lx, sic = 0x%x, next = 0x%lx\n", file->cluster, file->sectorInCluster, nextCluster);

    if (nextCluster >= 0xFF8) {
        printf("reached end of cluster sequence\n");
        return false;
    }

    Uint32 lba = clusterToLBA(nextCluster);

    if (!diskRead(&fat->disk,
                  lba + file->sectorInCluster,
                  1,
                  file->sector)) {
        printf("Failed to read file, lba %d\n", lba);
        return false;
    }

    file->cluster = nextCluster;
    file->remaining = fat->bpb.bytesPerSector;

    return true;
}

Uint16 getNextClusterNumber(Uint16 current)
{
    Uint16 index = (current * 3) / 2;
    Uint16 next = *(Uint16*)&fat->FAT[index];
    //printf("index = 0x%x, next=0x%x\n", index, next);
    if ((current % 2) == 0) {
        next &= 0xFFF;
    } else {
        next >>= 4;
    }

    return next;
}

Uint32 clusterToLBA(Uint32 cluster)
{
    Uint32 lba = fat->clusterBaseLBA + (cluster - 2) * fat->bpb.sectorsPerCluster;
    // TODO: Check lba calculations
    if (lba != cluster + 31) {
        printf("lba = %lx, cluster = %ls\n", lba, cluster);
        end();
    }
    return lba;
}

/*
 * Convert an 8.3 FAT filename to a string
 *
 * 8.3 filenames cannot contain spaces, so if they are there skip them as they are padding
 * The '.' and extension are optional
 * 
 * Caller is responsible for ensuring that out has space for at least 13 characters: 8 + '.' + 3 + null
 */
void convert8D3ToString(const char* name, char* out)
{
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

void convertStringTo8D3(const char* name, char* out)
{
    const char* originalName = name;
    int index;

    for (index = 0; *name && *name != '.' && index < 8; name++, ++index) {
        *out++ = toUpper(*name);
    }
    for (; index < 8; ++index) {
        *out++ = ' ';
    }

    if (*name != '.' && *name != '\0') {
        printf("Invalid file name component '%s'. Truncating\n", originalName);
        *out++ = ' ';
        *out++ = ' ';
        *out++ = ' ';
    } else if (*name == '\0') {
        *out++ = ' ';
        *out++ = ' ';
        *out++ = ' ';       
    } else {
        name++; // skip '.'
        for (index = 0; *name && index < 3; name++, ++index) {
            *out++ = toUpper(*name);
        }
        for (; index < 3; ++index) {
            *out++ = ' ';
        }
        if (*name != '\0') {
            printf("Invalid file name component '%s'. Truncating\n", originalName);
        }
    }
}

/*
 * Debugging functions
 */

void printFAT()
{
    // In FAT 12, cluster numbers in the FAT are 12 bits long
    // So multiply the cluster number by 3/2 to get a word index into the FAT
    // If the cluster number is even, we want the bottom 12 bits
    // Otherwise we want the top 12 bits

    for (Uint16 index = 0; index < 16; ++index) {
        printf("%x ", fat->FAT[index]);
    }
    printf("\n");

    for (Uint32 cluster = 0; cluster < 16; ++cluster) {
        Uint16 index = (cluster * 3) / 2;
        Uint16 value = *(Uint16*)&fat->FAT[index];
        if ((cluster % 2) == 0) {
            value &= 0xFFF;
        } else {
            value >>= 4;
        }
        printf("%x ", value);
    }
    printf("\n");
}

void printDirSector(File* dir)
{
    DirectoryEntry* entry = (DirectoryEntry*) dir->sector;
    printf("entry = %lx\n", entry);
    for (int ii = 0; ii < fat->bpb.bytesPerSector / sizeof(DirectoryEntry); ii++, entry++) {
        if (entry->name[0] == '\0') {
            continue;
        }
        printDirectoryEntry(entry);
    }
}

void printDirectoryEntry(DirectoryEntry* entry)
{
    printf("Directory Entry @ %lx:", entry);
    printFatName(entry->name);
    printf(" attr=%x first=%x size=%ld\n", entry->attributes, entry->firstClusterLow, entry->size);

}

void printFatName(char* fatName)
{
    char name[13]; // 8 + '.' + 3 + null
    convert8D3ToString(fatName, name);

    printf("'%s'", name);
}

void printFile(File* file)
{
    printf("File @ %lx: id = %d, isOpen=%d, isRoot=%d, isDir=%d first=%lx, clu=%lx, sic=%d, rem=%ld, pos=%ld, size=%ld, sec=%lx\n",
        file,
        file->id,
        file->isOpened,
        file->isRootDir,
        file->isDir,
        file->firstCluster,
        file->cluster,
        file->sectorInCluster,
        file->remaining,
        file->position,
        file->size,
        file->sector);
}
