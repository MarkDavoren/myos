#include "hal.h"
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <arch/i686/isr.h>
#include <arch/i686/irq.h>

void halInitialize()
{
    gdtInitialize();
    idtInitialize();
    isrInitialize();
    irqInitialize();
}
