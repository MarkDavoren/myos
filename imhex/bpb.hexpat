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
BPB bpb @ 0x4000;

struct EBR {
    // extended boot record
    u8 DriveNumber;
    u8 _Reserved;
    u8 Signature;
    u32 VolumeId;          // serial number, value doesn't matter
    u8 VolumeLabel[11];    // 11 bytes, padded with spaces
    u8 SystemId[8];
};
EBR ebr @ 0x4024;

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
FAT32_EBR ebr32 @ 0x4024;

fn nextFAT12Cluster(u16 curr) {
    u8 FAT[100] @ 0x4200;
    
    u16 index = (curr * 3) / 2;
    u16 val = FAT[index] | (FAT[index+1] << 8);
    
    if ((curr % 2) == 0) {
        return val & 0xFFF;
    } else {
        return val >> 4;
    }
};

fn main() {
    for (u8 ii=0, ii < 32, ii = ii + 1) {
        std::print("{:02x} {:03x}", ii, nextFAT12Cluster(ii));
    }
};