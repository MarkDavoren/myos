#pragma once

#include "isr.h"

typedef ISRRegisters IRQRegisters;
typedef void (*IRQHandler)(IRQRegisters* regs);

void irqInitialize();
void irqRegisterHandler(int irq, IRQHandler handler);
