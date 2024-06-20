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

typedef struct ISRRegistersT
{
    Uint32 ds;
    Uint32 edi, esi, ebp, useless, ebx, edx, ecx, eax;    // pusha
    Uint32 vectorNumber, error;
    Uint32 eip, cs, eflags;             // pushed automatically by CPU
    Uint32 esp, ss;                     // pushed automatically by CPU if there was a change in CPL
} ISRRegisters;

void __attribute((cdecl)) i686_ISR_0();
extern void* i686_isrTable[];

void test06(ISRRegisters* regs)
{
    printf("Test 06 handler called\n");
}

void isrInitialize()
{
    for (int ii = 0; ii < 256; ++ii) {
        idtSetGate(ii, i686_isrTable[ii], GDT_CODE_SEGMENT, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32BIT_INT);
    }
    // idtDisableGate(100); // Test that int 64h will cause a int Bh segment not present exception
    
    isrRegisterHandler(6, test06);  // Test dispatch to individual C handlers
}

void isrRegisterHandler(int vectorNumber, ISRHandler handler)
{
    isrHandlers[vectorNumber] = handler;
}

void __attribute__((cdecl)) isrHandler(ISRRegisters* regs)
{
    int vectorNumber = regs->vectorNumber;

    if (isrHandlers[vectorNumber] != NULL) {
        isrHandlers[vectorNumber](regs);

    } else {
        printf("Unhandled exception %xh '%s'\n", vectorNumber, exceptionMessages[vectorNumber]);

        printf("  eax=%x  ebx=%x  ecx=%x  edx=%x  esi=%x  edi=%x\n",
                regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);

        printf("  ebp=%x  eip=%x  eflags=%x  cs=%x  ds=%x\n",
                regs->ebp, regs->eip, regs->eflags, regs->cs, regs->ds);

        printf("  vectorNumber=%x  errorcode=%x\n",
                regs->vectorNumber, regs->error);
        
        if ((regs->cs & 0x3) != 0) {
            // Only print if we came from a different privilege level - bottom two bits of CS
            printf("  ss=%x  esp=%x\n", regs->ss, regs->esp);
        }

        printf("KERNEL PANIC!\n");

        i686_halt();
    }
}