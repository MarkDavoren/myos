#include "stdtypes.h"

typedef struct 
{
    Uint32 ds;
    Uint32 edi, esi, ebp, useless, ebx, edx, ecx, eax;    // pusha
    Uint32 interrupt, error;
    Uint32 eip, cs, eflags;             // pushed automatically by CPU
    Uint32 esp, ss;                     // pushed automatically by CPU if there was a change in CPL
} ISRRegisters;

typedef void (*ISRHandler)(ISRRegisters* regs);

void isrInitialize();
void isrRegisterHandler(int interrupt, ISRHandler handler);
