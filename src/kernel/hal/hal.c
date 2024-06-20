#include "hal.h"
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <arch/i686/isr.h>

void halInitialize()
{
    gdtInitialize();
    idtInitialize();
    isrInitialize();
}