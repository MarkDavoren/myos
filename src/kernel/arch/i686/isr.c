#include "stdtypes.h"
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "io.h"
#include "stdio.h"

ISRHandler isrHandlers[256];

static const char* exceptionMessages[] = {
    "Divide by zero error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception ",
    "",
    "",
    "",
    "",
    "",
    "",
    "Hypervisor Injection Exception",       // AMD??
    "VMM Communication Exception",          // AMD??
    "Security Exception",                   // AMD??
    ""
};

void __attribute((cdecl)) i686_ISR_0();
extern void* i686_isrTable[];

void test08(ISRRegisters* regs)
{
    printf("Test 08 handler called\n");
}

void isrInitialize()
{
    for (int ii = 0; ii < 256; ++ii) {
        idtSetGate(ii, i686_isrTable[ii], GDT_CODE_SEGMENT, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT_INT);
    }
    // idtDisableGate(100); // Test that int 64h will cause a int Bh segment not present exception
    
    isrRegisterHandler(8, test08);  // Test dispatch to individual C handlers
}

void isrRegisterHandler(int interrupt, ISRHandler handler)
{
    isrHandlers[interrupt] = handler;
}

void __attribute__((cdecl)) isrHandler(ISRRegisters* regs)
{
    int interrupt = regs->interrupt;

    if (isrHandlers[interrupt] != NULL) {
        isrHandlers[interrupt](regs);

    } else {
        printf("Unhandled exception %xh '%s'\n", interrupt, exceptionMessages[interrupt]);

        printf("  eax=%x  ebx=%x  ecx=%x  edx=%x  esi=%x  edi=%x\n",
                regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);

        printf("  esp=%x  ebp=%x  eip=%x  eflags=%x  cs=%x  ds=%x  ss=%x\n",
                regs->esp, regs->ebp, regs->eip, regs->eflags, regs->cs, regs->ds, regs->ss);

        printf("  interrupt=%x  errorcode=%x\n",
                regs->interrupt, regs->error);

        printf("KERNEL PANIC!\n");

        i686_halt();
    }
}