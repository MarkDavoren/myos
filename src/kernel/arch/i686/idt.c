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

void i686_IDT_SetGate(int interrupt, void* base, Uint16 segmentDescriptor, Uint8 flags)
{
    IDT[interrupt].baseLow = ((uint32_t)base) & 0xFFFF;
    IDT[interrupt].segmentSelector = segmentDescriptor;
    IDT[interrupt].reserved = 0;
    IDT[interrupt].flags = flags;
    IDT[interrupt].baseHigh = ((uint32_t)base >> 16) & 0xFFFF;
}

void i686_IDT_EnableGate(int interrupt)
{
    FLAG_SET(IDT[interrupt].flags, IDT_FLAG_PRESENT);
}

void i686_IDT_DisableGate(int interrupt)
{
    FLAG_UNSET(IDT[interrupt].flags, IDT_FLAG_PRESENT);
}

void i686_IDT_Initialize()
{
    i686_IDT_Load(&idtDescriptor);
}