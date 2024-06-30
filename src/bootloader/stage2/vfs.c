#include "vfs.h"

#include "stdtypes.h"
#include "fat.h"
#include "ext.h"

typedef struct {
    Bool    (*initialize)(Uint8 driveNumber, Partition* part);
    Handle  (*open)(const char* path);
    Uint32  (*read)(Handle fin, Uint32 count, void* buff);
    void    (*close)(Handle handle);
} Filesystem;

Filesystem filesystems[2] = {
    {
        fatInitialize,
        fatOpen,
        fatRead,
        fatClose
    },
    {
        extInitialize,
        extOpen,
        extRead,
        extClose
    }
};

int vType;

void vSetType(FilesystemType type)
{
    vType = type;
}

Bool vInitialize(Uint8 driveNumber, Partition* part)
{
    return filesystems[vType].initialize(driveNumber, part);
}

Handle vOpen(const char* path)
{
    return filesystems[vType].open(path);
}

Uint32 vRead(Handle fin, Uint32 count, void* buff)
{
    return filesystems[vType].read(fin, count, buff);
}

void vClose(Handle handle)
{
    return filesystems[vType].close(handle);
}