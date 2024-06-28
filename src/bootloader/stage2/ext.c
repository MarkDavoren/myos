#include "ext.h"
#include "stdtypes.h"
#include "stdio.h"
#include "memdefs.h"
#include "mbr.h"
#include "disk.h"
#include "utility.h"
#include "alloc.h"
#include "string.h"

typedef struct {
    Uint32      numInodes;
    Uint32      numBlocks;
    Uint32      numBlocksReservedForSuperuser;
    Uint32      numUnallocatedBlocks;
    Uint32      numUnallocatedInodes;
    Uint32      superblockBlock;
    Uint32      logBlockSize;
    Uint32      logFragmentSize;
    Uint32      numBlocksPerGroup;
    Uint32      numFragmentsPerGroup;
    Uint32      numInodesPerGroup;
    Uint32      lastMountTime;
    Uint32      lastWrittenTime;
    Uint16      mountsSinceCheck;
    Uint16      mountsUntilCheck;
    Uint16      ext2signature;
    Uint16      fileSystemState;
    Uint16      errorBehavior;
    Uint16      minorVersion;
    Uint32      lastCheckTime;
    Uint32      checkInterval;
    Uint32      osID;
    Uint32      majorVersion;
    Uint16      userIDForReservedBlock;
    Uint16      groupIDForReservedBlock;

    // Extended Superbock fields
    Uint32      firstNonreservedInode;
    Uint16      inodeSize;  // in bytes
    Uint16      backupSuperblockBlock;
    Uint32      optionalFeatures;
    Uint32      requiredFeatures;
    Uint32      readonlyIfUnsupported;
    Uint8       fileSystemID[16];
    Uint8       volumeName[16];
    Uint8       lastMountedPath[64];
    Uint32      compressionAlgorithmsUsed;
    Uint8       numBlocksPreallocateToFiles;
    Uint8       numBlocksPreallocateToDirectories;
    Uint16      unused;
    Uint8       journalID[16];
    Uint32      journalInode;
    Uint32      journalDevice;
    Uint32      headOrphanInodeList;
} __attribute__((packed)) Superblock;

typedef struct {
    Uint32      baBlockBitmap;
    Uint32      baInodeBitmap;
    Uint32      inodeTableBlock;
    Uint16      numUnallocatedBlocks;
    Uint16      numUnallocatedInodes;
    Uint16      numDirectories;
} __attribute__((packed)) BlockGroupDescriptor;

typedef struct {
    Uint16      typeAndPermissions;
    Uint16      userID;
    Uint32      sizeLow;
    Uint32      lastAccessTime;
    Uint32      creationTime;
    Uint32      lastModTime;
    Uint32      deletionTime;
    Uint16      groupID;
    Uint16      numHardLinks;
    Uint32      numDiskSectors;
    Uint32      flags;
    Uint32      oss1;
    Uint32      directBlock0;
    Uint32      directBlock1;
    Uint32      directBlock2;
    Uint32      directBlock3;
    Uint32      directBlock4;
    Uint32      directBlock5;
    Uint32      directBlock6;
    Uint32      directBlock7;
    Uint32      directBlock8;
    Uint32      directBlock9;
    Uint32      directBlock10;
    Uint32      directBlock11;
    Uint32      singlyIndirectBlock;
    Uint32      doublyIndirectBlock;
    Uint32      triplyIndirectBlock;
    Uint32      generationNUmber;
    Uint32      extendedAttributeBlock;
    Uint32      sizeHighOrDirACL;
    Uint32      fragmentBlock;
    Uint8       oss2[12];    
} __attribute__((packed)) Inode;

enum InodeType {
    IN_TAP_FIFO     = 0x1000,
    IN_TAP_CDEV     = 0x2000,
    IN_TAP_DIR      = 0x4000,
    IN_TAP_BDEV     = 0x6000,
    IN_TAP_FILE     = 0x8000,
    IN_TAP_SYMLINK  = 0xA000,
    IN_TAP_SOCKET   = 0xC000,
};

enum InodePermissions {
    IN_PERM_OX      = 0x001,
    IN_PERM_OW      = 0x002,
    IN_PERM_OR      = 0x004,
    IN_PERM_GX      = 0x008,
    IN_PERM_GW      = 0x010,
    IN_PERM_GR      = 0x020,
    IN_PERM_UX      = 0x040,
    IN_PERM_UW      = 0x080,
    IN_PERM_UR      = 0x100,
    IN_PERM_SGID    = 0x400,
    IN_PERM_SUID    = 0x800
};

#define MAX_FILENAME_LENGTH 255

typedef struct {
    Uint32      inodeNum;
    Uint16      size;
    Uint8       nameLength;
    Uint8       type;
    Uint8       name[MAX_FILENAME_LENGTH];
} __attribute__((packed)) DirectoryEntry;

enum DirectoryEntryType {
    DE_TYPE_UNKNOWN = 0,
    DE_TYPE_FILE    = 1,
    DE_TYPE_DIR     = 2,
    DE_TYPE_CDEV    = 3,
    DE_TYPE_BDEV    = 4,
    DE_TYPE_FIFO    = 5,
    DE_TYPE_SOCKET  = 6,
    DE_TYPE_SYMLINK = 7
};

typedef struct {
    Uint8       id;                 // Handle
    Bool        isOpened;           // If false then available for use
    Inode       inode;
    Uint32      position;           // Current position in bytes
    Uint32      currentBlockInFile;       // Number of block loaded into buffer
    void*       buffer;             // Point to current block buffer
} File;

#define MAX_HANDLES 10

typedef struct {
    Disk        disk;
    Uint32      blockSize;  // in bytes
    Uint16      inodeSize;  // in bytes
    Uint16      sectorsPerBlock;
    Uint32      numInodes;
    Uint32      numBlocks;
    Uint32      numBlocksPerGroup;
    Uint32      numInodesPerGroup;
    Uint32      inodeTableBlock;    // Assumes there is only one group
    File        files[MAX_HANDLES];
} ExtData;


Bool ext_readBlock(Disk* disk, Uint32 block, void* buffer);
File* ext_openFile(Uint32 iNum);
Handle ext_getFreeHandle();
Bool ext_readNextDirectoryEntry(File* file, DirectoryEntry* entry);
void ext_printDirectoryEntry(DirectoryEntry* entry);



ExtData ext;

#define SUPERBLOCK_DISK_ADDRESS 1024    // The superblock is always at 1024 bytes into the partition
#define SUPERBLOCK_LENGTH       1024    // and is always 1024 bytes long
#define SUPERBLOCK_SIGNATURE    0xef53
#define BGD_TABLE_SECTOR        2
#define ROOT_DIR_INODE          2

/*
 * This code will panic if there is more than one block group because support for it is not implemented
 */
Bool extInitialize(Uint8 driveNumber, Partition* part)
{
    // Initialize disk  -- Need to set disk.bytesPerSector later
    if (!diskInit(&ext.disk, driveNumber, part)) {
        printf("extInitialize: Failed to initialize disk %d\n", driveNumber);
        return false;
    }

    printf("Drive = %#x, Cylinders = %d, Heads = %d, Sectors = %d, offset = %d (%#x)\n",
        ext.disk.id,
        ext.disk.numCylinders,
        ext.disk.numHeads,
        ext.disk.numSectors,
        ext.disk.offset,
        ext.disk.offset);
    
    Superblock* sb = alloc(SUPERBLOCK_LENGTH);

    if (!diskExtRead(&ext.disk,
                    SUPERBLOCK_DISK_ADDRESS / ext.disk.bytesPerSector,
                    SUPERBLOCK_LENGTH / ext.disk.bytesPerSector,
                    (Uint8*) sb)) {
        printf("extInitialize: Failed to read superblock of disk %d\n", driveNumber);
        return false;
    }

    if (sb->ext2signature != SUPERBLOCK_SIGNATURE) {
        printf("extInitialize: Bad superblock signature %#x\n", sb->ext2signature);
        return false;
    }

    ext.blockSize = 1024 << sb->logBlockSize;
    ext.inodeSize = sb->inodeSize;
    ext.numInodes = sb->numInodes;
    ext.numBlocks = sb->numBlocks;
    ext.numBlocksPerGroup = sb->numBlocksPerGroup;
    ext.numInodesPerGroup = sb->numInodesPerGroup;

    ext.sectorsPerBlock = ext.blockSize / ext.disk.bytesPerSector;

    printf("extInit: bs=%#x, is=%#x, #i=%d, #b=%d, bpg=%d, ipg=%d\n",
            ext.blockSize,
            ext.inodeSize,
            ext.numInodes,
            ext.numBlocks,
            ext.numBlocksPerGroup,
            ext.numInodesPerGroup);
    
    free(sb); sb = NULL;

    Uint32 numGroups = divAndRoundUp(ext.numBlocks, ext.numBlocksPerGroup);

    if (numGroups > 1) {
        // This disk is bigger than any toy disks I should be working with!
        // Couldn't be bothered storing a whole table of BGDs
        printf("Support for ext2 partitions with multiple groups not implemented\n");
        return false;
    }

    BlockGroupDescriptor* bgd = alloc(ext.blockSize);

    // The BGD is in the next block after the SB.
    // If the block size is 1024 (the minimum) then that would be block 2, otherwise (for 2K or higher) it is 1
    Uint32 bgdBlockNum = ((SUPERBLOCK_DISK_ADDRESS + SUPERBLOCK_LENGTH < ext.blockSize) ? 1 : 2);
    if (!ext_readBlock(&ext.disk, bgdBlockNum, bgd)) {
        printf("extInitialize: Failed to read first sector of Block Group Descriptors of disk %d\n", driveNumber);
        return false;
    }

    ext.inodeTableBlock = bgd->inodeTableBlock;
    printf("inode table block = %#x\n", ext.inodeTableBlock);

    free(bgd); bgd = NULL;

    // Set up File table

    // Pre-allocate the buffers contiguously as they will never be freed
    // and would likely lead to fragmentation
    for (int ii = 0; ii < MAX_HANDLES; ++ii) {
    }
    for (int ii = 0; ii < MAX_HANDLES; ++ii) {
        ext.files[ii].id = ii;
        ext.files[ii].isOpened = false;
        ext.files[ii].buffer = alloc(ext.blockSize);
    }

    return true;
}

Bool ext_readBlock(Disk* disk, Uint32 block, void* buffer)
{
    return diskExtRead(disk, block * ext.sectorsPerBlock, ext.sectorsPerBlock, buffer);

}

Handle extOpen(const char* path)
{
    File* file = ext_openFile(ROOT_DIR_INODE);
    DirectoryEntry entry;
    while (ext_readNextDirectoryEntry(file, &entry)) {
        ext_printDirectoryEntry(&entry);
    }
}

File* ext_openFile(Uint32 iNum)
{
    printf("ext_openFile: %d\n", iNum);

    Handle handle = ext_getFreeHandle();
    if (handle == BAD_HANDLE) {
        return NULL;
    }
    File* file = &ext.files[handle];

    // Assuming there is only one block group

    Uint32 iBlock = ext.inodeTableBlock + ((iNum - 1) * ext.inodeSize) / ext.blockSize;
    Uint32 iOffset = ((iNum - 1) * ext.inodeSize) % ext.blockSize;

    printf("Dir block = %#x, offset = %#x\n", iBlock, iOffset);

    Uint8* inodeBlock = alloc(ext.blockSize);
    if (!ext_readBlock(&ext.disk, iBlock, inodeBlock)) {
        printf("ext_openFile: Failed to read inode block %#x\n", iBlock);
        return NULL;
    }
    printf("inodeBlock = %#x\n", inodeBlock);
    Inode* inode = (Inode*) (inodeBlock + iOffset);
    printf("size = %d, block0 = %#x\n", inode->sizeLow, inode->directBlock0);

    file->isOpened = true;
    //file->inode = *inode;     <- doesn't work!?!?
    memcpy(&file->inode, inode, sizeof(Inode));
    file->currentBlockInFile = UINT32_MAX; // Will cause a block load upon first read
    file->position = 0;

    printf("File: id = %d, size = %d, block0 = %#x, tap=%x\n",
            file->id,
            file->inode.sizeLow,
            file->inode.directBlock0,
            file->inode.typeAndPermissions);

    return file;
}

Handle ext_getFreeHandle()
{
    Handle handle;
    for (handle = 0; handle < MAX_HANDLES; ++handle) {
        if(!ext.files[handle].isOpened) {
            break;
        }
    }

    if (handle == MAX_HANDLES) {
        printf("Ran out of file handles\n");
        return BAD_HANDLE;
    }

    return handle;
}

Bool ext_readNextDirectoryEntry(File* file, DirectoryEntry* entry)
{
    printf("readNDE: pos = %#x, cbif = %#x, size = %d\n", file->position, file->currentBlockInFile, file->inode.sizeLow);

    if (file->position >= file->inode.sizeLow) {
        printf("EOF\n");
        return false;
    }

    Uint32 requiredBlockInFile = file->position / ext.blockSize;
    if (file->currentBlockInFile != requiredBlockInFile) {
        printf("Required block = %#x, current block = %#x\n", requiredBlockInFile, file->currentBlockInFile);
        Uint32 blockNum;
        if (requiredBlockInFile < 12) {
            blockNum = (&file->inode.directBlock0)[requiredBlockInFile];
        } else {
            panic("Indirect block fetches not implemented");
        }
        if (!ext_readBlock(&ext.disk, blockNum, file->buffer)) {
            printf("ext_openFile: Failed to read inode block %#x\n", blockNum);
            return false;
        }
        file->currentBlockInFile = requiredBlockInFile;
    }

    DirectoryEntry* de = (DirectoryEntry*) (file->buffer + file->position);
    entry->inodeNum = de->inodeNum;
    entry->size = de->size;
    entry->nameLength = de->nameLength;
    entry->type = de->type;
    memcpy(&entry->name, &de->name, de->nameLength);

    file->position += entry->size;
    file->position = align(file->position, 4);
    printf("readNDE: pos = %#x\n", file->position);

    return true;
}

#define MAX_FILENAME_LENGTH 255

void ext_printDirectoryEntry(DirectoryEntry* entry)
{
    char buff[MAX_FILENAME_LENGTH + 1];

    memcpy(buff, (Uint8*) &entry->name, entry->nameLength);
    buff[entry->nameLength] = '\0';
    printf("DE: inode = %d, size = %d, name length = %d, type = %#x, name = '%s'\n",
        entry->inodeNum, entry->size, entry->nameLength, entry->type, buff);
}