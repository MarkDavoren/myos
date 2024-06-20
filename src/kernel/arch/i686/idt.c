#include "idt.h"
#include "stdtypes.h"
#include "binary.h"

typedef struct
{
    Uint16 baseLow;
    Uint16 segmentSelector;
    Uint8  reserved;
    Uint8  flags;
    Uint16 baseHigh;
} __attribute__((packed)) IDTEntry;

typedef struct
{
    Uint16 limit;
    IDTEntry* ptr;
} __attribute__((packed)) IDTDescriptor;

IDTEntry IDT[256];

IDTDescriptor idtDescriptor = { sizeof(IDT) - 1, IDT };

void __attribute__((cdecl)) i686_IDT_Load(IDTDescriptor* idtDescriptor);

void idtSetGate(int vectorNumber, void* base, Uint16 segmentDescriptor, Uint8 flags)
{
    IDT[vectorNumber].baseLow = ((uint32_t)base) & 0xFFFF;
    IDT[vectorNumber].segmentSelector = segmentDescriptor;
    IDT[vectorNumber].reserved = 0;
    IDT[vectorNumber].flags = flags;
    IDT[vectorNumber].baseHigh = ((uint32_t)base >> 16) & 0xFFFF;
}

void idtEnableGate(int vectorNumber)
{
    FLAG_SET(IDT[vectorNumber].flags, IDT_FLAG_PRESENT);
}

void idtDisableGate(int vectorNumber)
{
    FLAG_UNSET(IDT[vectorNumber].flags, IDT_FLAG_PRESENT);
}

void idtInitialize()
{
    i686_IDT_Load(&idtDescriptor);
}