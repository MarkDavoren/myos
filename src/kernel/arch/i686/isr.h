#include "stdtypes.h"

typedef struct ISRRegistersT ISRRegisters;
typedef void (*ISRHandler)(ISRRegisters* regs);

void isrInitialize();
void isrRegisterHandler(int vectorNumber, ISRHandler handler);
