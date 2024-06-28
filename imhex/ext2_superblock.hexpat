struct Superblock {
    u32      numInodes;
    u32      numBlocks;
    u32      numBlocksReservedForSuperuser;
    u32      numUnallocatedBlocks;
    u32      numUnallocatedInodes;
    u32      superblockBlock;
    u32      logBlockSize;
    u32      logFragmentSize;
    u32      numBlocksInBlockGroup;
    u32      numFragmentsInBlockGroup;
    u32      numInodesInBlockGroup;
    u32      lastMountTime;
    u32      lastWrittenTime;
    u16      mountsSinceCheck;
    u16      mountsUntilCheck;
    u16      ext2signature;
    u16      fileSystemState;
    u16      errorBehavior;
    u16      minorVersion;
    u32      lastCheckTime;
    u32      checkInterval;
    u32      osID;
    u32      majorVersion;
    u16      userIDForReservedBlock;
    u16      groupIDForReservedBlock;

    // Extended Superbock fields
    u32      firstNonreservedInode;
    u16      inodeSize;  // in bytes
    u16      backupSuperblockBlock;
    u32      optionalFeatures;
    u32      requiredFeatures;
    u32      readonlyIfUnsupported;
    u8       fileSystemID[16];
    u8       volumeName[16];
    u8       lastMountedPath[64];
    u32      compressionAlgorithmsUsed;
    u8       numBlocksPreallocateToFiles;
    u8       numBlocksPreallocateToDirectories;
    u16      unused;
    u8       journalID[16];
    u32      journalInode;
    u32      journalDevice;
    u32      headOrphanInodeList;
};

Superblock sb @ 0x400;

struct BlockGroupDescriptor {
    u32      baBlockBitmap;
    u32      baInodeBitmap;
    u32      startInodes;
    u16      numUnallocatedBlocks;
    u16      numUnallocatedInodes;
    u16      numDirectories;
};

BlockGroupDescriptor bgd @ 0x1000;

struct Inode {
    u16      typeAndPermissions;
    u16      userID;
    u32      sizeLow;
    u32      lastAccessTime;
    u32      creationTime;
    u32      lastModTime;
    u32      deletionTime;
    u16      groupID;
    u16      numHardLinks;
    u32      numDiskSectors;    // Or is it numBlocks???
    u32      flags;
    u32      oss1;
    u32      directBlock0;
    u32      directBlock1;
    u32      directBlock2;
    u32      directBlock3;
    u32      directBlock4;
    u32      directBlock5;
    u32      directBlock6;
    u32      directBlock7;
    u32      directBlock8;
    u32      directBlock9;
    u32      directBlock10;
    u32      directBlock11;
    u32      singlyIndirectBlock;
    u32      doublyIndirectBlock;
    u32      triplyIndirectBlock;
    u32      generationNUmber;
    u32      extendedAttributeBlock;
    u32      sizeHighOrDirACL;
    u32      fragmentBlock;
    u8       oss2[12];    
};

Inode rootDirInode @ 0x5100;

struct DirectoryEntry {
    u32      inode;
    u16      size;
    u8       nameLength;
    u8       type;
    u8       name;
};

DirectoryEntry de @ 0x145000;
