#pragma once

#include "stdtypes.h"


void picInitialize(Uint8 pic1BaseIVN, Uint8 pic2BaseIVN, Bool autoEol);
Uint16 picGetMask();
void picSetMask(Uint16 mask);
void picMask(int irq);
void picUnmask(int irq);
void picDisable();
void picSendEndOfInterrupt(int irq);
void picSendSpecificEndOfInterrupt(int irq);
Uint16 picGetIRR();
Uint16 picGetISR();
Bool isPresent();
