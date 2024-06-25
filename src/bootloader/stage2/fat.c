#include "stdtypes.h"
#include "stdio.h"
#include "fat.h"
#include "memdefs.h"
#include "disk.h"
#include "ctype.h"
#include "string.h"
#include "utility.h"
#include "mbr.h"

#define MAX_HANDLES 10

typedef struct 
{
    Uint8  bootJumpInstruction[3];
    Uint8  oemIdentifier[8];
    Uint16 bytesPerSector;
    Uint8  sectorsPerCluster;
    Uint16 reservedSectors;
    Uint8  FatCount;
    Uint16 rootDirCount;        // This field is zero in FAT32. The root dir is a regular cluster-based directory
    Uint16 totalSectors16;      // Must be zero for FAT32. If zero use totalSectors32
    Uint8  mediaDescriptorType;
    Uint16 sectorsPerFat16;     // FAT12/16 use this field. For FAT32 this field must be zero; use sectorsPerFat32
    Uint16 sectorsPerTrack;
    Uint16 heads;
    Uint32 hiddenSectors;
    Uint32 totalSectors32;      // For FAT12/16, use this value if totalSectors16 == 0. For FAT32 always use this field
} __attribute__((packed)) BiosParameterBlock;

/*
 * There is nothing of interest in here. If we need the EBR, it will always be EBR32
 */
typedef struct {
    Uint8  driveNumber;
    Uint8  reservedNT;
    Uint8  signature;
    Uint32 volumeID;
    Uint8  volumeLabel[11];
    Uint8  fsType[8];
} __attribute__((packed)) EBR1216;

typedef struct {
    Uint32 sectorsPerFat32;     // FAT32 uses this field and ignores sectorsPerFat16 whcih must be zero
    Uint16 flags;
    Uint16 fsVersion;
    Uint32 rootCluster;
    Uint16 fsInfoSector;
    Uint16 bootSecondary;
    Uint8  reserved32[12];
    Uint8  driveNumber;
    Uint8  reservedNT;
    Uint8  signature;
    Uint32 volumeID;
    Uint8  volumeLabel[11];
    Uint8  fsType[8];
} __attribute__((packed)) EBR32;

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

enum FatAttributes
{
    FAT_ATTRIBUTE_READ_ONLY         = 0x01,
    FAT_ATTRIBUTE_HIDDEN            = 0x02,
    FAT_ATTRIBUTE_SYSTEM            = 0x04,
    FAT_ATTRIBUTE_VOLUME_ID         = 0x08,
    FAT_ATTRIBUTE_DIRECTORY         = 0x10,
    FAT_ATTRIBUTE_ARCHIVE           = 0x20,
    FAT_ATTRIBUTE_LFN               = FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID
};

typedef enum {
    FAT12,
    FAT16,
    FAT32
} FatType;

/*
 * There is one File per possible handle
 * They are stored in fat->files
 * Each File has a designated space to hold 1 sector of the opened file
 * which is contained in the space pointed to by fat->dataSectors
 *   file->sector = fat->dataSectors[handle * fat->bpb.bytesPerSector]
  */
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
    Uint32      size;               // Maximum position in bytes (zero for directories)
    Uint8*      sector;             // Point to current sector buffer
} File;

/*
 * The FatData structure holds all info required to read files from a FAT filesystem
 *
 * A FAT system will have either EBR1216 (FAT12 or FAT16) or EBR32 (FAT32)
 * We work out which by calling getFatType and storing the value in fatType
 * 
 * The File Allocation Table can be huge so we read just two sectors at a time
 * and load new ones on demand overwriting the FAT "cache"
 * 
 * For FAT12 and FAT16, rootDirLBA points to the special area holding the root dir
 * For FAT32, the root directory is just a regular file with starting cluster in ebr32.rootCluster
 * 
 * There is a File for each possible file handle
 * Each file has a sector of memory in the dataSectors array
 */

typedef struct {
    Disk        disk;               // Disk geometry details
    BiosParameterBlock  bpb;        // BIOS Parameter Block
    FatType     fatType;            // One of FAT12, FAT16, or FAT32
    union {
        EBR1216 ebr1216;
        EBR32   ebr32;
    } ebr;
    Uint32      sectorsPerFat;      // Calculated from BPB and EBR
    Uint32      totalSectors;       // Calculated from BPB and EBR
    Uint32      currentFATSector;   // The currently loaded FAT sector
    Uint8*      FAT;                // where the currently loaded FAT sector is
    Uint32      fatLBA;             // LBA of FAT
    Uint32      rootDirLBA;         // LBA of root directory
    Uint32      dataLBA;            // LBA of data sectors
    Uint8*      dataSectors;        // Address in mem buffer holding a sector for each File entry
    File        files[MAX_HANDLES]; // Array of Files. One for each possible handle value
} FatData;

static FatData* fat = (FatData*) FAT_MEM_ADDRESS;

#define FAT_BUFFER_SIZE 2

/*
 * Forward declarations
 */

FatType getFatType(FatData* fat);
File* openRootDir();
File* openFile(DirectoryEntry* entry);
Uint32 readFile(File* file, Uint32 count, Uint8* buff);
void closeFile(File* file);
Handle getFreeHandle();
const char* getComponent(const char* path, char* component, int limit);
Bool findFileInDirectory(const char* name, File* dir, DirectoryEntry* foundEntry);
Bool readDirEntry(File* dir, DirectoryEntry* entry);
Bool readNextSector(File* dir);
Bool readNextSectorFromFAT1216RootDir(File* dir);
Bool readNextSectorFromFile(File* file);
Uint32 getNextClusterNumber(Uint32 current);
Uint32 clusterToLBA(Uint32 cluster);
void convert8D3ToString(const char* name, char* out);
void convertStringTo8D3(const char* name, char* out);

void printBPB(BiosParameterBlock* bpb);
void printEBR32(EBR32* ebr);
void printDirSector(File* dir);
void printFAT();
void printDirectoryEntry(DirectoryEntry* entry);
void printFatName(char* fatName);
void printFile(File* file);

/*
 * Public functions
 */

Bool fatInitialize(Uint8 driveNumber, Partition* part)
{
    // Initialize disk  -- Need to set disk.bytesPerSector later
    if (!diskInit(&fat->disk, driveNumber, part)) {
        printf("Failed to initialize disk %d\n", driveNumber);
        return false;
    }
    // printf("Cylinders = %d, Heads = %d, Sectors = %d, offset = %d\n",
    //     fat->disk.numCylinders,
    //     fat->disk.numHeads,
    //     fat->disk.numSectors,
    //     fat->disk.offset);

    // Read MBR into a temporary location and copy BPB from there to where we want it
    Uint8* tmp = (Uint8*) (FAT_MEM_ADDRESS + sizeof(FatData));
    if (!diskRead(&fat->disk, 0, 1, tmp)) {
        printf("Failed to read MBR of disk %d\n", driveNumber);
        return false;
    }

    fat->bpb = *(BiosParameterBlock*) tmp;
    fat->ebr.ebr32 = *(EBR32*) (tmp + sizeof(BiosParameterBlock));

    fat->disk.bytesPerSector = fat->bpb.bytesPerSector;

    fat->sectorsPerFat = (fat->bpb.sectorsPerFat16 == 0) ? fat->ebr.ebr32.sectorsPerFat32 : fat->bpb.sectorsPerFat16;
    fat->totalSectors = (fat->bpb.totalSectors16 == 0) ? fat->bpb.totalSectors32 : fat->bpb.totalSectors16;
    printBPB(&fat->bpb);
    printEBR32(&fat->ebr.ebr32);
    fat->fatType = getFatType(fat);
    printf("Found FAT Type = %d\n", fat->fatType);

    fat->FAT = (Uint8*) (FAT_MEM_ADDRESS + sizeof(FatData));   // Hold FAT in mem right after fat
    fat->currentFATSector = 0xFFFFFFFF; // Force a cache miss and load

    fat->dataSectors = (Uint8*) FAT_MEM_ADDRESS;
    Uint32 offset = align((Uint32)(sizeof(FatData) + FAT_BUFFER_SIZE * fat->bpb.bytesPerSector),
                          (Uint32)fat->bpb.bytesPerSector);
    fat->dataSectors += offset;

    // Set up convenience LBAs
    fat->fatLBA = fat->bpb.reservedSectors;

    fat->rootDirLBA = fat->fatLBA + fat->bpb.FatCount * fat->sectorsPerFat;

    Uint32 rootDirSizeInBytes = fat->bpb.rootDirCount * sizeof(DirectoryEntry);
    Uint32 rootDirSizeInSectors = roundup(rootDirSizeInBytes, fat->bpb.bytesPerSector);

    fat->dataLBA = fat->rootDirLBA + rootDirSizeInSectors;

    printf("rootDirLBA = 0x%x, dataLBA = 0x%x\n", fat->rootDirLBA, fat->dataLBA);

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

FatType getFatType(FatData* fat)
{
    // In order to allow use of FAT32 on smaller file systems, we cheat and just look at rootDirCount
    if (fat->bpb.rootDirCount == 0) {
        return FAT32;
    }

    /*
     * This is the official way to determine which FAT file system is in use
     * https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
     * Note: If the filesystem is FAT12 or 16 then this algorithm will not access ebr
     */

    Uint32 rootDirSectors = roundup((fat->bpb.rootDirCount * sizeof(DirectoryEntry)), fat->bpb.bytesPerSector);
    Uint32 dataSectors = fat->totalSectors - (fat->bpb.reservedSectors + (fat->bpb.FatCount * fat->sectorsPerFat) + rootDirSectors);

    Uint32 numClusters = dataSectors / fat->bpb.sectorsPerCluster;

    printf("GFT: rds=%d, spf=%d, ts=%d, ds=%d, nc=%d\n",
        rootDirSectors, fat->sectorsPerFat, fat->totalSectors, dataSectors, numClusters);

    if (numClusters < 4085) {
        return FAT12;
    } else if (numClusters < 65525) {
        return FAT16;
    } else {
        return FAT32;
    }
}

bool readFATSectorForIndex(Uint32 index)
{
    Uint32 sector = index / fat->bpb.bytesPerSector;

    if (sector >= fat->sectorsPerFat) {
        printf("readFATSectorForIndex: Requested sector invalid: index=%x, sector=%x, spf=%x\n", index, sector, fat->sectorsPerFat);
        return false;
    }

    if (sector == fat->currentFATSector) {
        // The requested FAT sector is already in cache
        return true;
    }

    // Normally read the requested sector plus one so we don't have to worry
    // about a cluster crossing the sector boundary.
    // Only read 1 sector if we are requested to get the last one
    int count = FAT_BUFFER_SIZE;
    if (sector == fat->sectorsPerFat - 1) {
        count = 1;
    }
    if (!diskBigRead(
            &fat->disk,
            fat->fatLBA + sector,
            count,
            (Uint8*) fat->FAT)) {
        printf("Failed to read sectors %d-%d FAT\n", sector, sector+count-1);
        return false;
    }
    //printFAT();

    return true;
}

File* openRootDir()
{
    File* dir;

    int handle = getFreeHandle();
    if (handle == BAD_HANDLE) {
        return NULL;
    }

    dir = &fat->files[handle];
    dir->id = handle;
    dir->isOpened = true;
    dir->isRootDir = true;
    dir->isDir = true;
    dir->cluster = 0;
    dir->sectorInCluster = 0;
    dir->remaining = 0;         // This will cause first read to load first sector
    dir->position = 0;
    dir->size = 0;              // Size is always zero for a directory
    dir->sector = &fat->dataSectors[handle * fat->bpb.bytesPerSector]; // Set up space, read will be on demand

    if (fat->fatType == FAT32) {
        dir->firstCluster = fat->ebr.ebr32.rootCluster;
    } else {
        dir->firstCluster = 0;                      // N/A for the root dir
    }

    return dir;
}

/*
 * Open a file or directory (just not a FAT12/16 root directory)
 */
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

/*
 * Read count bytes from a file or directory (just not a FAT12/16 root directory)
 */
Uint32 readFile(File* file, Uint32 count, Uint8* buff)
{
    printf("Requesting to read %ld bytes from file %d\n", count, file->id);
    printFile(file);
    
    if (!file->isDir) {
        Uint32 remaining = file->size - file->position;    // remaining bytes in whole file
        if (remaining == 0) {
            printf("Reached EOF\n");
            return 0;
        }

        if (count > remaining) {
            count = remaining;
            printf("Reduced count to %d\n", count);
        }
    }

    Uint32 totalRead = 0;
    while (count > 0) {
        if (file->remaining == 0) {                     // remaining bytes in sector
            if(!readNextSector(file)) {
                // This can happen when reading from directories since size == 0
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
        buff += actual;
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
        if (entry.name[0] == 0xE5) {
            continue;   // name starting with E5 signals entry is free
        }
        if (entry.name[0] == '\0') {
            break;      // name starting with '\0' signals all remaining entries are free
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
    printf("RDE: ");
    printFile(dir);

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
    if (dir->isRootDir && fat->fatType != FAT32) {
        return readNextSectorFromFAT1216RootDir(dir);
    } else {
        return readNextSectorFromFile(dir);
    }
}

Bool readNextSectorFromFAT1216RootDir(File* dir)
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

Uint32 getNextClusterNumber(Uint32 current)
{
    Uint32 index;
    Uint32 next;

    switch(fat->fatType) {
        case FAT12: index = current + (current / 2); break;
        case FAT16: index = current * 2; break;
        case FAT32: index = current * 4; break;
    }

    readFATSectorForIndex(index);

    switch (fat->fatType) {
    case FAT12:
        next = *(Uint16*) &fat->FAT[index];
        if ((current % 2) == 0) {
            next &= 0xFFF;
        } else {
            next >>= 4;
        }
        break;
    
    case FAT16:
        next = *(Uint16*) &fat->FAT[index];
        //printf("FAT16: index=%d, FAT = %p, [%x %x]\n", index, fat->FAT, fat->FAT[index], fat->FAT[index+1]);
        break;
    
    case FAT32:
        next = *(Uint32*) &fat->FAT[index];
        break;
    }

    //printf("current = %x, next = %x\n", current, next);
    return next;
}

Uint32 clusterToLBA(Uint32 cluster)
{
    Uint32 lba = fat->dataLBA + (cluster - 2) * fat->bpb.sectorsPerCluster;
    // TODO: Check lba calculations
    if (lba != cluster + 31) {
        printf("lba = %lx, cluster = %lx\n", lba, cluster);
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

void printBPB(BiosParameterBlock* bpb)
{
    printf("BPB: bps=%d, spc=%d, rs=%d, fc=%d, rdc=%d\n",
        bpb->bytesPerSector,
        bpb->sectorsPerCluster,
        bpb->reservedSectors,
        bpb->FatCount,
        bpb->rootDirCount);
    printf("BPB: ts=%d, spt=%d, h=%d, hs=%d, ts32=%d\n",
        bpb->totalSectors16,
        bpb->sectorsPerFat16,
        bpb->sectorsPerTrack,
        bpb->heads,
        bpb->hiddenSectors,
        bpb->totalSectors32);
}

void printEBR32(EBR32* ebr)
{
    printf("EBR32: spf32=%d, flags=%x, ver=%x, rc=%x, fsinfo=%d\n",
        ebr->sectorsPerFat32,
        ebr->flags,
        ebr->fsVersion,
        ebr->rootCluster,
        ebr->fsInfoSector);
    printf("EBR32: bootsec=%d, drive=%x, sig=%x, volID=%x\n",
        ebr->bootSecondary,
        ebr->driveNumber,
        ebr->signature,
        ebr->volumeID);
}

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

    // for (Uint32 cluster = 0; cluster < 16; ++cluster) {
    //     Uint16 index = (cluster * 3) / 2;
    //     Uint16 value = *(Uint16*)&fat->FAT[index];
    //     if ((cluster % 2) == 0) {
    //         value &= 0xFFF;
    //     } else {
    //         value >>= 4;
    //     }
    //     printf("%x ", value);
    // }
    // printf("\n");
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
