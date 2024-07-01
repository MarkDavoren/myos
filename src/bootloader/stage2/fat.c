#include "stdtypes.h"
#include "stdio.h"
#include "fat.h"
#include "memdefs.h"
#include "disk.h"
#include "ctype.h"
#include "string.h"
#include "utility.h"
#include "mbr.h"
#include "alloc.h"

#define MAX_HANDLES 3
#define FAT_BUFFER_SIZE 2
#define MBR_DISK_ADDRESS 0
#define MBR_SIZE_SECTORS 1

/*
 * FAT Filesystem data structures
 */

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
 * MYOS FAT Filesystem data structures
 */

/*
 * There is one File per possible handle
 * They are stored in fat.files[handle]
 * Each File contains info needed for open and read functions
 *   including a pointer to a buffer on the heap which holds one sector of data
 */

typedef struct {
    Uint8       id;                 // Handle
    Bool        isOpened;           // If false then available for use
    Bool        isRootDir;          // Flag for special handling to read the root directory
    Bool        isDir;              // True if a directory
    Uint32      firstCluster;       // First cluster
    Uint32      cluster;            // Current cluster
    Uint8       sectorInCluster;    // Current sector within the current cluster
    Uint32      sectorInBuffer;     // Sector LBA that is currently loaded in buffer
    Uint32      position;           // Current position in bytes
    Uint32      size;               // Maximum position in bytes (zero for directories)
    Uint8*      buffer;             // Point to current sector buffer
} File;

/*
 * The FatData structure holds all info required to read files from a FAT filesystem
 * including an array for files (open or available)
 *
 * A FAT system will have either EBR1216 (FAT12 or FAT16) or EBR32 (FAT32)
 * We work out which by calling fat_getFatType and storing the value in fatType
 * 
 * The File Allocation Table can be huge so we read just two sectors at a time
 * and load new ones on demand overwriting the FAT "cache". We store two sectors
 * so we don't need to worry about reading across a sector boundary
 * 
 * For FAT12 and FAT16, rootDirLBA points to the special area on the disk holding the root dir
 * For FAT32, the root directory is just a regular file with starting cluster in ebr32.rootCluster
 */

typedef struct {
    Disk        disk;               // Disk geometry details
    FatType     fatType;            // One of FAT12, FAT16, or FAT32
    Uint32      endClusterMarker;   // 0xFF8, 0xFFF8, or 0x0FFFFFF8 
    Uint32      rootCluster;        // If FAT32
    Uint16      bytesPerSector;     // From the BPB
    Uint8       sectorsPerCluster;  // From the BPB
    Uint32      sectorsPerFat;      // Calculated from BPB and EBR
    Uint32      totalSectors;       // Calculated from BPB and EBR
    Uint32      currentFATSector;   // The currently loaded FAT sector
    Uint8*      FAT;                // where the currently loaded FAT sector is
    Uint32      fatLBA;             // LBA of FAT
    Uint32      rootDirLBA;         // LBA of root directory
    Uint32      dataLBA;            // LBA of data sectors
    File        files[MAX_HANDLES]; // Array of Files. One for each possible handle value
} FatData;

/*
 * Master FatData object
 */
FatData fat;

/*
 * Forward declarations
 */

FatType fat_getFatType(FatData* fat, BiosParameterBlock* bpb);
File*   fat_openRootDir();
File*   fat_openFile(DirectoryEntry* entry);
Uint32  fat_readFile(File* file, Uint32 count, Uint8* buff);
void    fat_closeFile(File* file);
Handle  fat_getFreeHandle();
Bool    fat_findFileInDirectory(const char* name, File* dir, DirectoryEntry* foundEntry);
Bool    fat_readDirEntry(File* dir, DirectoryEntry* entry);
Bool    fat_readNextSector(File* dir);
Bool    fat_readNextSectorFromFAT1216RootDir(File* dir);
Bool    fat_readNextSectorFromFile(File* file);
Uint32  fat_getNextClusterNumber(Uint32 current);
Uint32  fat_clusterToLBA(Uint32 cluster);
void    fat_convert8D3ToString(const char* name, char* out);
void    fat_cconvertStringTo8D3(const char* name, char* out);

void    fat_printBPB(BiosParameterBlock* bpb);
void    fat_printEBR32(EBR32* ebr);
void    fat_printDirSector(File* dir);
void    fat_printFAT();
void    fat_printDirectoryEntry(DirectoryEntry* entry);
void    fat_printFATName(char* fatName);
void    fat_printFile(File* file);

// ###############################################
//      Public functions
// ###############################################

/*
 * Initialize the filesystem including
 *  the disk
 *  the FatData structure
 *  storage for the FAT and file buffers
 */
Bool fatInitialize(Uint8 driveNumber, Partition* part)
{
    // Initialize disk
    if (!diskInit(&fat.disk, driveNumber, part)) {
        printf("Failed to initialize disk %d\n", driveNumber);
        return false;
    }

    printf("Drive %#x, Cylinders = %d, Heads = %d, Sectors = %d, bps = %d, offset = %d (%#x)\n",
        fat.disk.id,
        fat.disk.numCylinders,
        fat.disk.numHeads,
        fat.disk.numSectors,
        fat.disk.bytesPerSector,
        fat.disk.offset,
        fat.disk.offset);

    // Grab what we need from the boot sector
    BiosParameterBlock* bpb = alloc(fat.disk.bytesPerSector);
    if (!diskExtRead(&fat.disk, MBR_DISK_ADDRESS, MBR_SIZE_SECTORS, (Uint8*) bpb)) {
        printf("Failed to read boot sector of disk %d\n", driveNumber);
        panic("Failed to load FAT boot sector");
        return false;
    }
    printf("bpb = %p\n", bpb);
    printHeap();
    fat_printBPB(bpb);

    EBR32* ebr = (EBR32*) (((Uint8*)bpb) + sizeof(BiosParameterBlock));
    fat_printEBR32(ebr);

    // Validation checks
    if (bpb->bytesPerSector == 0
     || bpb->sectorsPerCluster == 0
     || bpb->FatCount == 0) {
        panic("Invalid FAT partition");
    }

    if (bpb->bytesPerSector != fat.disk.bytesPerSector) {
        printf("Disk params and BPB disagree on bytes per sector: disk: %d, bpb: %d\n",
               fat.disk.bytesPerSector,
               fat.bytesPerSector);
               panic("Fatal error!");
    }

    // Initialize fat data struct
    fat.bytesPerSector = bpb->bytesPerSector;
    fat.sectorsPerCluster = bpb->sectorsPerCluster;
    fat.sectorsPerFat = (bpb->sectorsPerFat16 == 0) ? ebr->sectorsPerFat32 : bpb->sectorsPerFat16;
    fat.totalSectors = (bpb->totalSectors16 == 0) ? bpb->totalSectors32 : bpb->totalSectors16;
    fat.rootCluster = ebr->rootCluster;

    fat.fatType = fat_getFatType(&fat, bpb);
    printf("Found FAT Type = %d\n", fat.fatType);
    switch (fat.fatType)
    {
        case FAT12: fat.endClusterMarker = 0xFF8; break;
        case FAT16: fat.endClusterMarker = 0xFFF8; break;
        case FAT32: fat.endClusterMarker = 0x0FFFFFF8; break;
        default:
            panic("Unknown FAT type for end cluster sequence marker");
            return false;
    }

    // Set up convenience LBAs
    fat.fatLBA = bpb->reservedSectors;
    fat.rootDirLBA = fat.fatLBA + bpb->FatCount * fat.sectorsPerFat;
    Uint32 rootDirSizeInBytes = bpb->rootDirCount * sizeof(DirectoryEntry);
    Uint32 rootDirSizeInSectors = divAndRoundUp(rootDirSizeInBytes, fat.bytesPerSector);
    fat.dataLBA = fat.rootDirLBA + rootDirSizeInSectors;

    printf("fatLBA = %#x, rootDirLBA = %#x, dataLBA = %#x\n", fat.fatLBA, fat.rootDirLBA, fat.dataLBA);

    free(bpb); bpb = NULL;

    // Setup space to hold a couple of sectors of the FAT
    fat.FAT = alloc(FAT_BUFFER_SIZE * fat.bytesPerSector);
    fat.currentFATSector = UINT32_MAX; // Force a cache miss and load

    // Set up File table
    for (int ii = 0; ii < MAX_HANDLES; ++ii) {
        fat.files[ii].id = ii;
        fat.files[ii].isOpened = false;
        fat.files[ii].buffer = alloc(fat.bytesPerSector);
    }

    return true;
}

/*
 * Open a file given a path
 *
 * Returns a handle. BAD_HANDLE on error
 */
Handle fatOpen(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        printf("Failed to open file with empty path\n");
        return BAD_HANDLE;
    }

    const char* originalPath = path;
    
    File* file = fat_openRootDir();

    if (file == NULL) {
        printf("Failed to open the root directory\n");
        return BAD_HANDLE;
    }

    if (path[0] == '/') {
        path++; // Skip leading '/'
    }

    while (*path != '\0') {
        char component[13]; // 8 + '.' + 3 + null
        path = getComponent(path, component, '/', sizeof(component));
        if (*path != '\0' && *path != '/') {
            printf("Failed to open file '%s': Component '%s' is too long\n", originalPath, component);
            fat_closeFile(file);
            return BAD_HANDLE;
        }

        DirectoryEntry entry;
        if (!fat_findFileInDirectory(component, file, &entry)) {
            printf("Failed to open file '%s': Could not find '%s'\n", originalPath, component);
            fat_closeFile(file);
            return BAD_HANDLE;
        }

        //printf("fatOpen: ");
        //fat_printDirectoryEntry(&entry);

        fat_closeFile(file);                     // close parent directory
        file = fat_openFile(&entry);  // open child (file or dir)
        //printf("fatOpen: ");
        //fat_printFile(file);

        if (*path == '\0') {
            break;
        }

        path++;     // skip '/'

        if (!file->isDir) {
            printf("Failed to open file '%s': Component '%s' is not a directory\n", originalPath, component);
            fat_closeFile(file);
        }
    }
    //printf("fatOpen returning id=%d\n", file->id);
    fat_printFile(file);
    return file->id;
}

/*
 * Read count bytes from handle into buff
 *
 * Returns number of bytes read
 * Returning zero bytes indicates an error though usually that is end of file
 */
Uint32 fatRead(Handle handle, Uint32 count, void* buff)
{
    return fat_readFile(&fat.files[handle], count, buff);
}

/*
 * Close handle
 */
void fatClose(Handle handle)
{
    fat_closeFile(&fat.files[handle]);
}

// ###############################################
//      Private functions
// ###############################################

FatType fat_getFatType(FatData* fat, BiosParameterBlock* bpb)
{
    // In order to allow use of FAT32 on smaller file systems, we cheat and just look at rootDirCount
    if (bpb->rootDirCount == 0) {
        return FAT32;
    }

    /*
     * This is the official way to determine which FAT file system is in use
     * https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
     * Note: If the filesystem is FAT12 or 16 then this algorithm will not access ebr
     */

    Uint32 rootDirSectors = divAndRoundUp((bpb->rootDirCount * sizeof(DirectoryEntry)), fat->bytesPerSector);
    Uint32 dataSectors = fat->totalSectors - (bpb->reservedSectors + (bpb->FatCount * fat->sectorsPerFat) + rootDirSectors);

    Uint32 numClusters = dataSectors / bpb->sectorsPerCluster;

    printf("GFT: rds=%d, spf=%d, ts=%d, ds=%d, nc=%d\n",
        rootDirSectors, fat->sectorsPerFat, fat->totalSectors, dataSectors, numClusters);

    if (numClusters < 4085) {
        return FAT12;
    } else if (numClusters < 65525) {
        return FAT16;
    } else {
        return FAT32;
    }

    return -1;
}

bool readFATSectorForIndex(Uint32 index)
{
    Uint32 sector = index / fat.bytesPerSector;

    if (sector >= fat.sectorsPerFat) {
        printf("readFATSectorForIndex: Requested sector invalid: index=%x, sector=%x, spf=%x\n", index, sector, fat.sectorsPerFat);
        return false;
    }

    if (sector == fat.currentFATSector) {
        // The requested FAT sector is already in cache
        return true;
    }

    //printf("Loading FAT %#x for index = %#x\n", fat.fatLBA + sector, index);
    if (!diskExtRead(
            &fat.disk,
            fat.fatLBA + sector,
            FAT_BUFFER_SIZE,            // TODO: Danger of reading past FAT end
            (Uint8*) fat.FAT)) {
        printf("Failed to read sectors %d-%d FAT\n", sector, sector+FAT_BUFFER_SIZE);
        panic("Can't read FAT");
        return false;
    }
    sector = fat.currentFATSector;
    //fat_printFAT();

    return true;
}

File* fat_openRootDir()
{
    File* dir;

    int handle = fat_getFreeHandle();
    if (handle == BAD_HANDLE) {
        return NULL;
    }

    dir = &fat.files[handle];
    dir->id = handle;
    dir->isOpened = true;
    dir->isRootDir = true;
    dir->isDir = true;
    dir->cluster = 0;
    dir->sectorInCluster = 0;
    dir->sectorInBuffer = UINT32_MAX;   // This will force a sector load on first read attempt
    dir->position = 0;
    dir->size = 0;                      // Size is always zero for a directory

    if (fat.fatType == FAT32) {
        dir->firstCluster = fat.rootCluster;
    } else {
        dir->firstCluster = 0;                      // N/A for the root dir
    }

    return dir;
}

/*
 * Open a file or directory (just not a FAT12/16 root directory)
 */
File* fat_openFile(DirectoryEntry* entry)
{
    Handle handle = fat_getFreeHandle();
    if (handle == BAD_HANDLE) {
        return NULL;
    }

    File* file = &fat.files[handle];

    file->id = handle;
    file->isOpened = true;
    file->isRootDir = false;
    file->isDir = (entry->attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    file->firstCluster = entry->firstClusterLow + (((Uint32)entry->firstClusterHigh) << 16);
    file->cluster = 0;                          // Cause first read to read from firstCluster
    file->sectorInCluster = 0;
    file->sectorInBuffer = UINT32_MAX;          // This will force a sector load on first read attempt
    file->position = 0;
    file->size = entry->size;

    printf("Opened handle %d\n", handle);
    return file;
}

/*
 * Read count bytes from a file or directory (just not a FAT12/16 root directory)
 */
Uint32 fat_readFile(File* file, Uint32 count, Uint8* buff)
{
    //printf("fat_readFile: Requesting to read %ld bytes from file %d\n", count, file->id);
    //fat_printFile(file);

    // If this a regular file then read as far as size.
    // If this is a directory then read to the end of the last sector in the cluster sequence
    if (!file->isDir) {
        Uint32 remainingInFile = file->size - file->position;
        if (remainingInFile < count) {
            count = remainingInFile;
        }
    }

    if (count == 0) {
        printf("EOF\n");
        return 0;
    }

    Uint32 bytesRead = 0;

    while (bytesRead < count) {
        if (file->position / fat.bytesPerSector != file->sectorInBuffer) {
            /*
             * FAT filesystems are designed for sequential reads; they use a list of clusters
             * To support seeking to other positions in the file would require traversing the list
             * from the beginning each time we don't have the correct sector to locate the correct cluster
             * 
             * For our purposes, we will always be reading sequentially.
             * So if the sectorInBuffer isn't what we want then we assume the next one will be!
             */
            if(!fat_readNextSector(file)) {
                return bytesRead;
            }
        }

        Uint32 bytesToRead = count - bytesRead;
        Uint32 positionInSector = file->position % fat.bytesPerSector;
        if ( bytesToRead > fat.bytesPerSector - positionInSector) {
            bytesToRead = fat.bytesPerSector - positionInSector;
        }

        //printf("Copying %ld bytes, from %lx to %lx\n", bytesToRead, file->buffer + positionInSector, buff + bytesRead);
        memcpy(buff + bytesRead, file->buffer + positionInSector, bytesToRead);

        file->position  += bytesToRead;
        bytesRead += bytesToRead;
    }

    //printf("Total read = %ld\n", bytesRead);
    return bytesRead;
}

void fat_closeFile(File* file)
{
    file->isOpened = false;
}

Handle fat_getFreeHandle()
{
    Handle handle;
    for (handle = 0; handle < MAX_HANDLES; ++handle) {
        if(!fat.files[handle].isOpened) {
            break;
        }
    }

    if (handle == MAX_HANDLES) {
        printf("Ran out of file handles\n");
        return BAD_HANDLE;
    }

    return handle;
}

/*
 * Assumes dir is newly opened and hence position == 0
 */
Bool fat_findFileInDirectory(const char* name, File* dir, DirectoryEntry* foundEntry)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    char fatName[11];
    fat_cconvertStringTo8D3(name, fatName);

    DirectoryEntry entry;
    while (fat_readDirEntry(dir, &entry)) {
        if (entry.name[0] == 0xE5) {
            continue;   // name starting with E5 signals entry is free
        }
        if (entry.name[0] == '\0') {
            break;      // name starting with '\0' signals all remaining entries are free
        }
        //fat_printFile(dir);
        printf("Comparing ");
        fat_printFATName(entry.name);
        printf(" to ");
        fat_printFATName(fatName);
        printf("\n");
        if (memcmp(entry.name, fatName, 11) == 0) {
            *foundEntry = entry;
            fat_printDirectoryEntry(foundEntry);
            return true;
        }
    }

    return false;
}

Bool fat_readDirEntry(File* dir, DirectoryEntry* entry)
{
    // printf("RDE: ");
    // fat_printFile(dir);

    if (dir->position / fat.bytesPerSector != dir->sectorInBuffer) {
        if(!fat_readNextSector(dir)) {
            return false;
        }
    }

    *entry = *((DirectoryEntry*) (dir->buffer + (dir->position % fat.bytesPerSector)));
    dir->position  += sizeof(DirectoryEntry);

    //fat_printDirectoryEntry(entry);

    return true;
}

Bool fat_readNextSector(File* dir)
{
    if (dir->isRootDir && fat.fatType != FAT32) {
        return fat_readNextSectorFromFAT1216RootDir(dir);
    } else {
        return fat_readNextSectorFromFile(dir);
    }
}

Bool fat_readNextSectorFromFAT1216RootDir(File* dir)
{
    Uint16 sector = dir->position / fat.bytesPerSector;

    if (!diskExtRead(&fat.disk,
                     fat.rootDirLBA + sector,
                     1,
                     dir->buffer)) {
        printf("Failed to read Root directory, sector %d\n", sector);
        return false;
    }

    dir->sectorInBuffer = sector;

    return true;
}

Bool fat_readNextSectorFromFile(File* file)
{
    Uint32 nextCluster;

    if (file->cluster == 0) {
        nextCluster = file->firstCluster;
        file->sectorInCluster = 0;
    } else if (file->sectorInCluster < fat.sectorsPerCluster - 1) {
        nextCluster = file->cluster;
        file->sectorInCluster++;
    } else {
        nextCluster = fat_getNextClusterNumber(file->cluster);
        file->sectorInCluster = 0;
    }
    //printf("Current cluster = %#x, sic = %#x, next = %#x\n", file->cluster, file->sectorInCluster, nextCluster);

    if (nextCluster >= fat.endClusterMarker) {
        printf("reached end of cluster sequence\n");
        return false;
    }

    Uint32 lba = fat_clusterToLBA(nextCluster);

    if (!diskExtRead(&fat.disk,
                     lba + file->sectorInCluster,
                     1,
                     file->buffer)) {
        printf("Failed to read file, lba %d\n", lba);
        return false;
    }

    file->cluster = nextCluster;
    file->sectorInBuffer++;

    return true;
}

Uint32 fat_getNextClusterNumber(Uint32 current)
{
    Uint32 index;
    Uint32 next;

    switch(fat.fatType) {
        case FAT12: index = current + (current / 2); break;
        case FAT16: index = current * 2; break;
        case FAT32: index = current * 4; break;
    }

    readFATSectorForIndex(index);   // This will get us the right sector
    index %= fat.bytesPerSector;    // This will get us the right byte offset into that sector

    switch (fat.fatType) {
    case FAT12:
        next = *(Uint16*) &fat.FAT[index];
        if ((current % 2) == 0) {
            next &= 0xFFF;
        } else {
            next >>= 4;
        }
        break;
    
    case FAT16:
        next = *(Uint16*) &fat.FAT[index];
        //printf("FAT16: index=%d, FAT = %p, [%x %x]\n", index, fat.FAT, fat.FAT[index], fat.FAT[index+1]);
        break;
    
    case FAT32:
        next = *(Uint32*) &fat.FAT[index];
        break;
    }

    //printf("current = %x, next = %x\n", current, next);
    return next;
}

Uint32 fat_clusterToLBA(Uint32 cluster)
{
    Uint32 lba = fat.dataLBA + (cluster - 2) * fat.sectorsPerCluster;
    //printf("cluster = %#x => lba = %#x\n", cluster, lba);

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
void fat_convert8D3ToString(const char* name, char* out)
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

void fat_cconvertStringTo8D3(const char* name, char* out)
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

// ###############################################
//      Debugging functions
// ###############################################

void fat_printBPB(BiosParameterBlock* bpb)
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

void fat_printEBR32(EBR32* ebr)
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

void fat_printFAT()
{
    // In FAT 12, cluster numbers in the FAT are 12 bits long
    // So multiply the cluster number by 3/2 to get a word index into the FAT
    // If the cluster number is even, we want the bottom 12 bits
    // Otherwise we want the top 12 bits

    for (Uint16 index = 0; index < 16; ++index) {
        printf("%x ", fat.FAT[index]);
    }
    printf("\n");

    // for (Uint32 cluster = 0; cluster < 16; ++cluster) {
    //     Uint16 index = (cluster * 3) / 2;
    //     Uint16 value = *(Uint16*)&fat.FAT[index];
    //     if ((cluster % 2) == 0) {
    //         value &= 0xFFF;
    //     } else {
    //         value >>= 4;
    //     }
    //     printf("%x ", value);
    // }
    // printf("\n");
}

void fat_printDirSector(File* dir)
{
    DirectoryEntry* entry = (DirectoryEntry*) dir->buffer;
    printf("entry = %lx\n", entry);
    for (int ii = 0; ii < fat.bytesPerSector / sizeof(DirectoryEntry); ii++, entry++) {
        if (entry->name[0] == '\0') {
            continue;
        }
        fat_printDirectoryEntry(entry);
    }
}

void fat_printDirectoryEntry(DirectoryEntry* entry)
{
    printf("Directory Entry @ %lx:", entry);
    fat_printFATName(entry->name);
    printf(" attr=%x first=%x size=%ld\n", entry->attributes, entry->firstClusterLow, entry->size);

}

void fat_printFATName(char* fatName)
{
    char name[13]; // 8 + '.' + 3 + null
    fat_convert8D3ToString(fatName, name);

    printf("'%s'", name);
}

void fat_printFile(File* file)
{
    printf("File @ %lx: id = %d, isOpen=%d, isRoot=%d, isDir=%d, first=%lx, clu=%lx, sic=%d, sib=%ld, pos=%ld, size=%ld, buff=%lx\n",
        file,
        file->id,
        file->isOpened,
        file->isRootDir,
        file->isDir,
        file->firstCluster,
        file->cluster,
        file->sectorInCluster,
        file->sectorInBuffer,
        file->position,
        file->size,
        file->buffer);
}
