#include "stdtypes.h"
#include "stdio.h"
#include "fat.h"
#include "memdefs.h"
#include "disk.h"
#include "ctype.h"
#include "string.h"
#include "utility.h"

#define MAX_HANDLES 10

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
} DirectoryEntry;

#pragma pack(pop)

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
    Uint8 far*  sector;             // Buffer to hold current sector
} File;

typedef struct {
    Disk disk;
    BootSector bpb;
    Uint8 far* FAT;
    Uint32 rootDirLBA;
    File files[MAX_HANDLES];
} FatData;

static FatData far* fatData = (FatData far*)FAT_MEM_ADDRESS;
static Uint8 far* fatSectors;

/*
 * Forward declarations
 */

File far* openRootDir();
File far* openFile(DirectoryEntry* entry);
Uint32 readFile(File far* file, Uint32 count, Uint8 far* buff);
void closeFile(File far* file);
Handle getFreeHandle();
const char* getComponent(const char* path, char* component, int limit);
Bool findFileInDirectory(const char* name, File far* dir, DirectoryEntry* foundEntry);
Bool readDirEntry(File far* dir, DirectoryEntry* entry);
Bool readNextSector(File far* dir);
Bool readNextSectorFromRootDir(File far* dir);
Bool readNextSectorFromFile(File far* file);
Uint32 getNextClusterNumber(Uint16 current);
Uint32 clusterToLBA(Uint32 cluster);
void convert8D3ToString(const char far* name, char* out);
void convertStringTo8D3(const char* name, char* out);

void printDirSector(File far* dir);
void printFAT();
void printDirectoryEntry(DirectoryEntry far* entry);
void printFatName(char far* fatName);
void printFile(File far* file);

/*
 * Public functions
 */

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

    // Read MBR into a temporary location and copy BPB from there to where we want it
    Uint8 far* tmp = FAT_MEM_ADDRESS + sizeof(FatData);
    if (!diskRead(&fatData->disk, 0, 1, tmp)) {
        printf("Failed to read MBR of disk %d\r\n", driveNumber);
        return false;
    }
    fatData->bpb = *(BootSector far*) tmp;

    // Load FAT
    // Load the entire first FAT. TODO: This is not scalable, but will be fine for 1.44MB floppy

    fatData->FAT = FAT_MEM_ADDRESS + sizeof(FatData);   // Hold FAT in mem right after fatData
    if (!diskRead(
            &fatData->disk,
            fatData->bpb.ReservedSectors,   // Start of FAT on disk
            fatData->bpb.SectorsPerFat,     // Read the entire first FAT. Ignore the others as they are copies
            (Uint8 far*) fatData->FAT)) {
        printf("Failed to read FAT of disk %d\r\n", driveNumber);
        return false;
    }

    fatSectors = FAT_MEM_ADDRESS;
    Uint32 offset = align((Uint32)(sizeof(FatData) + fatData->bpb.SectorsPerFat * fatData->bpb.BytesPerSector),
                          (Uint32)fatData->bpb.BytesPerSector);
    fatSectors += offset;

    // Save LBA of root directory and entries/sector
    fatData->rootDirLBA = fatData->bpb.ReservedSectors + fatData->bpb.FatCount * fatData->bpb.SectorsPerFat;

    // Initialize Files
    for (int ii = 0; ii < MAX_HANDLES; ++ii) {
        fatData->files[ii].isOpened = false;
    }

    return true;
}
    DirectoryEntry entry;

Handle fatOpen(const char* path)
{
    const char* originalPath = path;
    File far* dir;
    File far* file;

    if (path == NULL || path[0] == '\0') {
        return BAD_HANDLE;
    }

    if (path[0] != '/') {
        printf("Failed to open file '%s': Path does not begin with a '/'\r\n", originalPath);
        return BAD_HANDLE;
    }

    path++; // Skip root '/'
    dir = openRootDir();

    if (*path == '\0') {
        // Caller asked to open "/"
        file = dir;
    }
 
    while (*path != '\0') {
        char component[13]; // 8 + '.' + 3 + null
        path = getComponent(path, component, sizeof(component));
        if (*path != '\0' && *path != '/') {
            printf("Failed to open file '%s': Component '%s' is too long\r\n", originalPath, component);
            closeFile(dir);
            return BAD_HANDLE;
        }


        if (!findFileInDirectory(component, dir, &entry)) {
            printf("Failed to open file '%s': Could not find '%s'\r\n", originalPath, component);
            closeFile(dir);
            return BAD_HANDLE;
        }

        printf("fatOpen: ");
        printDirectoryEntry(&entry);

        closeFile(dir);                     // close parent directory
        File far* file = openFile(&entry);  // open child (file or dir)

        if (*path == '\0') {
            break;
        }

        path++;     // skip '/'
        dir = file;
        if (!dir->isDir) {
            printf("Failed to open file '%s': Component '%s' is not a directory\r\n", originalPath, component);
            closeFile(dir);
        }
    }
    printf("fatOpen returning\r\n");
    return file->id;
}

Uint32 fatRead(Handle handle, Uint32 count, void far* buff)
{
    return readFile(&fatData->files[handle], count, buff);
}

void fatClose(Handle handle)
{
    closeFile(&fatData->files[handle]);
}

/*
 * Private functions
 */

File far* openRootDir()
{
    int handle = getFreeHandle();

    File far* dir = &fatData->files[handle];
    dir->id = handle;
    dir->isOpened = true;
    dir->isRootDir = true;
    dir->isDir = true;
    dir->firstCluster = 0;                      // N/A for the root dir
    dir->cluster = 0;                           // N/A for the root dir
    dir->sectorInCluster = 0;                   // N/A for the root dir
    dir->remaining = 0;                         // This will cause first read to load first sector
    dir->position = 0;                          // For directories, position is current entry#
    dir->size = fatData->bpb.DirEntryCount * fatData->bpb.BytesPerSector;
    dir->sector = &fatSectors[handle * fatData->bpb.BytesPerSector]; // Set up space, read will be on demand

    return dir;
}

File far* openFile(DirectoryEntry* entry)
{
    Handle handle = getFreeHandle();
    if (handle == BAD_HANDLE) {
        return NULL;
    }

    File far* file = &fatData->files[handle];

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
    file->sector = &fatSectors[handle * fatData->bpb.BytesPerSector]; // Set up space, read will be on demand

    printf("Opened handle %d\r\n", handle);
    return file;
}

Uint32 readFile(File far* file, Uint32 count, Uint8 far* buff)
{
    printf("Requesting to read %ld bytes from file\r\n", count);
    
    Uint32 remaining = file->size - file->position;    // remaining bytes in whole file
    if (remaining == 0) {
        printf("Reached EOF\r\n");
        return 0;
    }

    if (count > remaining) {
        count = remaining;
        printf("Reduced count to %d\r\n", count);
    }

    Uint32 totalRead = 0;
    while (count > 0) {
        if (file->remaining == 0) {                     // remaining bytes in sector
            if(!readNextSector(file)) {
                // This shouldn't happen: Can't get next sector, but we are not at the EOF!?
                return totalRead;
            }
        }

        Uint32 actual = count;
        if (file->remaining < count) {
            actual = file->remaining;
        }

        Uint16 offset = fatData->bpb.BytesPerSector - file->remaining;
        printf("Copying %ld bytes, from %lx to %lx\r\n", actual, file->sector + offset, buff);
        memcpy(buff, file->sector + offset, actual);

        file->position  += actual;
        file->remaining -= actual;
        totalRead += actual;
        count -= actual;
    }

    printf("Total read = %ld\r\n", totalRead);
    return totalRead;
}

void closeFile(File far* file)
{
    file->isOpened = false;
}

Handle getFreeHandle()
{
    Handle handle;
    for (handle = 0; handle < MAX_HANDLES; ++handle) {
        if(!fatData->files[handle].isOpened) {
            break;
        }
    }

    if (handle == MAX_HANDLES) {
        printf("Ran out of file handles\r\n");
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
Bool findFileInDirectory(const char* name, File far* dir, DirectoryEntry* foundEntry)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    char fatName[11];
    convertStringTo8D3(name, fatName);

    DirectoryEntry entry;
    for (int ii = 0; readDirEntry(dir, &entry) && ii < 5; ++ii) {
        if (entry.name[0] == '\0') {
            continue;
        }
        printf("Comparing ");
        printFatName(entry.name);
        printf(" to ");
        printFatName(fatName);
        printf("\r\n");
        if (memcmp(entry.name, fatName, 11) == 0) {
            *foundEntry = entry;
            return true;
        }
    }

    return false;
}

Bool readDirEntry(File far* dir, DirectoryEntry* entry)
{
    printf("RDE: ");
    printFile(dir);

    int remaining = dir->size - dir->position;      // remaining bytes in whole file
    if (remaining == 0) {
        printf("Searched through all entries\r\n");
        return false;
    }

    if (dir->remaining == 0) {                      // remaining bytes in sector
        if(!readNextSector(dir)) {
            return false;
        }
    }

    *entry = *((DirectoryEntry far*) (dir->sector + (dir->position % fatData->bpb.BytesPerSector)));
    dir->position  += sizeof(DirectoryEntry);
    dir->remaining -= sizeof(DirectoryEntry);

    //printDirectoryEntry(entry);

    return true;
}

Bool readNextSector(File far* dir)
{
    if (dir->isRootDir) {
        return readNextSectorFromRootDir(dir);
    } else {
        return readNextSectorFromFile(dir);
    }
}

Bool readNextSectorFromRootDir(File far* dir)
{
    Uint16 sector = dir->position / fatData->bpb.BytesPerSector;

    if (!diskRead(&fatData->disk,
                  fatData->rootDirLBA + sector,
                  1,
                  dir->sector)) {
        printf("Failed to read Root directory, sector %d\r\n", sector);
        return false;
    }

    dir->remaining = fatData->bpb.BytesPerSector;

    return true;
}

Bool readNextSectorFromFile(File far* file)
{
    Uint32 nextCluster;
    if (file->cluster == 0) {
        nextCluster = file->firstCluster;
    } else {
        nextCluster = getNextClusterNumber(file->cluster);
    }
    printf("Current cluster = 0x%lx, next = 0x%lx\r\n", file->cluster, nextCluster);

    if (nextCluster >= 0xFF8) {
        printf("reached end of cluster sequence\r\n");
        return false;
    }

    Uint32 lba = clusterToLBA(nextCluster);

    // TODO: Handle multiple sectors per cluster
    if (!diskRead(&fatData->disk,
                  lba,
                  1,
                  file->sector)) {
        printf("Failed to read file, lba %d\r\n", lba);
        return false;
    }

    file->cluster = nextCluster;
    file->remaining = fatData->bpb.BytesPerSector;

    return true;
}

Uint32 getNextClusterNumber(Uint16 current)
{
    Uint16 index = (current * 3) / 2;
    Uint16 next = *(Uint16 far*)&fatData->FAT[index];
    //printf("index = 0x%x, next=0x%x\r\n", index, next);
    if ((current % 2) == 0) {
        next &= 0xFFF;
    } else {
        next >>= 4;
    }

    return next;
}

Uint32 clusterToLBA(Uint32 cluster)
{
    return cluster + 31;    // TODO: Only works for floppy geometry and FAT12
}

/*
 * Convert an 8.3 FAT filename to a string
 *
 * 8.3 filenames cannot contain spaces, so if they are there skip them as they are padding
 * The '.' and extension are optional
 * 
 * Caller is responsible for ensuring that out has space for at least 13 characters: 8 + '.' + 3 + null
 */
void convert8D3ToString(const char far* name, char* out)
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
        printf("Invalid file name component '%s'. Truncating\r\n", originalName);
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
            printf("Invalid file name component '%s'. Truncating\r\n", originalName);
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
        printf("%x ", fatData->FAT[index]);
    }
    printf("\r\n");

    // For FAT12, clusterHigh is always zero, so 16 bits is enough
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
}

void printDirSector(File far* dir)
{
    DirectoryEntry far* entry = (DirectoryEntry far*) dir->sector;
    printf("entry = %lx\r\n", entry);
    for (int ii = 0; ii < fatData->bpb.BytesPerSector / sizeof(DirectoryEntry); ii++, entry++) {
        if (entry->name[0] == '\0') {
            continue;
        }
        printDirectoryEntry(entry);
    }
}

void printDirectoryEntry(DirectoryEntry far* entry)
{
    printf("Directory Entry @ %lx:", entry);
    printFatName(entry->name);
    printf(" attr=%x first=%x size=%ld\r\n", entry->attributes, entry->firstClusterLow, entry->size);

}

void printFatName(char far* fatName)
{
    char name[13]; // 8 + '.' + 3 + null
    convert8D3ToString(fatName, name);

    printf("'%s'", name);
}

void printFile(File far* file)
{
    printf("File @ %lx: id = %d, isOpen=%d, isRoot=%d, isDir=%d first=%lx, clu=%lx, sic=%d, rem=%ld, pos=%ld, size=%ld, sec=%lx\r\n",
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
