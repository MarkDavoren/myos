#include "irq.h"
#include "stdtypes.h"
#include "pic.h"
#include "io.h"
#include "stdio.h"

#define PIC_BASE_IVN 0x20
#define MAX_NUM_IRQS 16

IRQHandler irqHandlers[MAX_NUM_IRQS];

void irqRegisterHandler(int irq, IRQHandler handler)
{
    irqHandlers[irq] = handler;
    picUnmask(irq);
}

void irqCommonHandler(ISRRegisters* regs)
{
    int irq = regs->vectorNumber - PIC_BASE_IVN;

    if (irqHandlers[irq] != NULL) {
        irqHandlers[irq](regs);
    } else {
        printf("Unhandled IRQ %d, ISR = %x, IRR = %x\n", irq, picGetISR(), picGetIRR());
    }

    picSendEndOfInterrupt(irq);
}

void irqInitialize()
{
    picInitialize(PIC_BASE_IVN, PIC_BASE_IVN + 8, true);

    for (int ii = 0; ii < MAX_NUM_IRQS; ++ii) {
        isrRegisterHandler(PIC_BASE_IVN + ii, irqCommonHandler);
    }

    i686_enableInterrupts();
}
