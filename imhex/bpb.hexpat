#include <std/io.pat>

struct BPB {
    u8  bootJumpInstruction[3];
    u8  oemIdentifier[8];
    u16 bytesPerSector;
    u8  sectorsPerCluster;
    u16 reservedSectors;
    u8  FatCount;
    u16 dirEntriesCount;
    u16 totalSectors;
    u8  mediaDescriptorType;
    u16 sectorsPerFat;
    u16 sectorsPerTrack;
    u16 heads;
    u32 hiddenSectors;
    u32 largeSectorCount;
};


struct EBR {
    // extended boot record
    u8 DriveNumber;
    u8 _Reserved;
    u8 Signature;
    u32 VolumeId;          // serial number, value doesn't matter
    u8 VolumeLabel[11];    // 11 bytes, padded with spaces
    u8 SystemId[8];
};


struct FAT32_EBR {
    u32 SectorsPerFat;
    u16 Flags;
    u16 FatVersion;
    u32 RootDirectoryCluster;
    u16 FSInfoSector;
    u16 BackupBootSector;
    u8 _Reserved32[12];

    // extended boot record
    u8 DriveNumber;
    u8 _Reserved16;
    u8 Signature;
    u32 VolumeId;          // serial number, value doesn't matter
    u8 VolumeLabel[11];    // 11 bytes, padded with spaces
    u8 SystemId[8];
};