#include "pic.h"
#include "stdtypes.h"
#include "io.h"

/*
 * The Programmable Interrupt Controller (PIC) 8259A
 *
 * https://wiki.osdev.org/8259_PIC
 * https://pdos.csail.mit.edu/6.828/2005/readings/hardware/8259A.pdf
 *
 * The PIC is used to prioritize and manage interrupts from multiple devices acting as a gatekeeper to the CPU
 * It may be cascaded allowing upto 64 interrupt request lines.
 * 
 * We will assume a pair of PICs with PIC1 being the master and PIC2 the slave
 * PIC1 will manage IRQs 0-7 (excluding IRQ2 which connects to the slave)
 * PIC2 will manage IRQs 8-15
 * 
 * To configure a PIC, commands are sent to its command and data ports
 * First the PIC needs to be initialized. This requires four commands: ICW1 to ICW4
 * We can instruct the PICs to mask (ignore) certain IRQs using OCW1
 * We can send the End of Interrupt via OCW2
 * We can read the PIC registers via OCW3
 */

#define PIC1_COMMAND_PORT           0x20
#define PIC1_DATA_PORT              0x21
#define PIC2_COMMAND_PORT           0xA0
#define PIC2_DATA_PORT              0xA1

/*
 * Initialization Command Word 1 (ICW1)
 *
 * 0 - IC4      If set, the PIC expects to receive ICW4 during initializtion
 * 1 - SNGL     If set, only 1 PIC is in the system
 *              If unset, the PIC is cascaded with slave PICs and ICW3 must be sent to controller
 * 2 - ADI      Call Address Interval. Set = 4, unset = 8
 * 3 - LTIM     If set, operate in level triggered mode
 *              If unset, operate in edge triggered mode
 * 4            Must always be 1
 * 5-7          Ignored on x86, set to 0
 */

enum {
    PIC_ICW1_ICW4_REQUIRED  = 0x01,

    PIC_ICW1_CASCADE        = 0x00,
    PIC_ICW1_SINGLE         = 0x02,

    PIC_ICW1_INTERVAL8      = 0x00,
    PIC_ICW1_INTERVAL4      = 0x04,

    PIC_ICW1_EDGE           = 0x00,
    PIC_ICW1_LEVEL          = 0x08,

    PIC_ICW1_INITIALIZE     = 0x10

    // Bits 5-8 should be 0 for x86

} PIC_ICW1;

/*
 * Initialization Control Word 4 (ICW4)
 *
 * 0 - uPM      if set, PIC is in 80x86 mode
 *              if unset, PIC is in MCS-80/85 mode
 * 1 - AEOI     If set, on last interrupt acknowledge pulse, controller automatically performs 
 *                  end of interrupt operation
 * 2 - M/S      Only used if BUF is set. If set, PIC is buffer master; otherwise, PIC is buffer slave
 * 3 - BUF      If set, controller operates in buffered mode
 * 4 - SFNM     Specially fully nested mode; used in systems with large number of cascaded controllers
 * 5-7          Reserved, set to 0
 */

enum {
    PIC_ICW4_MCS80              = 0X00,
    PIC_ICW4_X86                = 0X01,

    PIC_ICW4_NORMAL_EOI         = 0X00,
    PIC_ICW4_AUTO_EOI           = 0X02,

    PIC_ICW4_NON_BUFFERED       = 0X00,
    PIC_ICW4_BUFFERED_SLAVE     = 0X08,
    PIC_ICW4_BUFFERED_MASTER    = 0X0C,
    
    PIC_ICW4_SFNM               = 0X10
} PIC_ICW4;

/*
 * Operation Command Word 1 (OCW1)
 *
 * 0-7          A bit mask to disable interrupt
 */


/*
 * Operation Command Word 2 (OCW2)
 *
 * 0-2          The interrupt number affected
 * 3-4          Always 0
 * 5-7          Encoded command
 *                  001 (0x20) Non-specific end of interrupt
 *                  011 (0x60) Specific end of interrupt specified in bits 0-2
 *                  010 (0x40) No operation
 *                  Not using other commands
 */

enum {
    PIC_OCW2_NONSPECIFIC_EOI    = 0x20,
    PIC_OCW2_SPECIFIC_EOI       = 0x60,
    PIC_OCW2_NOP                = 0x40
} PIC_OCW2;

/*
 * Operation Command Word 3 (OCW3)
 * 0-1          Read register commands
 *                  02 = Interrupt Service Register (ISR)
 *                  03 = Interrupt Read Register (IRR)
 * 2 P          If set, use polling
 * 3            Always 1
 * 4            Always 0
 * 5-6          Special Mask Mode (not used)
 * 7            Always 0
 */

enum {
    PIC_OCW3_READ_IRR = 0x08 | 0x02,
    PIC_OCW3_READ_ISR = 0x08 | 0x03,
} PIC_OCW3;

/*
 * Initialize the PICs
 *
 * The PICs are hardwired in cascade. PIC1 is the master and PIC is the slave wired to IR2 of the master
 * Each PIC manages 8 interrupts and require 8 interrupt vector numbers in the IDT
 * PIC1's IVNs start at pic1BaseIVN, likewise PIC2 starts at pic2BaseIVN
 * 
 * On older machines it was necessary to give the PIC some time to process a command. Hence the iowait
 */
void picInitialize(Uint8 pic1BaseIVN, Uint8 pic2BaseIVN, Bool autoEol)
{
    // Initialization Control Word 1
    i686_outb(PIC1_COMMAND_PORT, PIC_ICW1_ICW4_REQUIRED | PIC_ICW1_INITIALIZE);
    i686_iowait();
    i686_outb(PIC2_COMMAND_PORT, PIC_ICW1_ICW4_REQUIRED | PIC_ICW1_INITIALIZE);
    i686_iowait();

    // Initialization Control Word 2
    // PIC1 will "own" interrupt vector numbers pic1BaseIVN to pic1BaseIVN + 7
    i686_outb(PIC1_DATA_PORT, pic1BaseIVN);
    i686_iowait();
    // PIC2 will "own" interrupt vector numbers pic2BaseIVN to pic2BaseIVN + 7
    i686_outb(PIC2_DATA_PORT, pic2BaseIVN);
    i686_iowait();

    // Initialization Control Word 3
    i686_outb(PIC1_DATA_PORT, 0x4); // Tell PIC1 that it has a slave at IRQ2. Nth bit corresponds to IRn
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, 0x2); // Tell PIC2 its cascade identity is IRQ2. Bits 0..2 encode octal value
    i686_iowait();

    // Initialization Control Word 4
    Uint8 icw4 = PIC_ICW4_X86;
    if (autoEol) {
        icw4 |= PIC_ICW4_AUTO_EOI;
    }
    i686_outb(PIC1_DATA_PORT, icw4);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, icw4);
    i686_iowait();

    // clear data registers
    i686_outb(PIC1_DATA_PORT, 0);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, 0);
    i686_iowait();

    // Disable all interrupts until device drivers enable their own
    picDisable();
}

/*
 * Operation Command Word 1
 *
 * Modify the interrupt mask
 */
static Uint16 picCachedMask;

Uint16 picGetMask()
{
    picCachedMask = i686_inb(PIC1_DATA_PORT) | (i686_inb(PIC2_DATA_PORT) << 8);
    return picCachedMask;
}

void picSetMask(Uint16 mask)
{
    picCachedMask = mask;
    i686_outb(PIC1_DATA_PORT, picCachedMask & 0xFF);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, picCachedMask >> 8);
    i686_iowait();
}

void picMask(int irq)
{
    picSetMask(picCachedMask | (1 << irq));
}

void picUnmask(int irq)
{
    picSetMask(picCachedMask & ~(1 << irq));
}

void picDisable()
{
    picSetMask(0xFFFF);
}

/*
 * Operation Command Word 2
 *
 * Send End of Interrupt
 */

void picSendEndOfInterrupt(int irq)
{
    i686_outb(PIC1_COMMAND_PORT, PIC_OCW2_NONSPECIFIC_EOI);
    if (irq >= 8) {
        i686_outb(PIC2_COMMAND_PORT, PIC_OCW2_NONSPECIFIC_EOI);
    }
}

void picSendSpecificEndOfInterrupt(int irq)
{
    if (irq >= 8) {
        i686_outb(PIC1_COMMAND_PORT, PIC_OCW2_SPECIFIC_EOI | 2);
        i686_outb(PIC2_COMMAND_PORT, PIC_OCW2_SPECIFIC_EOI | (irq - 8));
    } else {
        i686_outb(PIC1_COMMAND_PORT, PIC_OCW2_SPECIFIC_EOI | irq);
    }
}

/*
 * Operation Command Word 3
 *
 * Get PIC registers
 */

Uint16 picGetIRR()
{
    i686_outb(PIC1_COMMAND_PORT, PIC_OCW3_READ_IRR);
    i686_outb(PIC2_COMMAND_PORT, PIC_OCW3_READ_IRR);
    return ((Uint16)i686_inb(PIC1_COMMAND_PORT)) | (((Uint16)i686_inb(PIC2_COMMAND_PORT)) << 8);
}

Uint16 picGetISR()
{
    i686_outb(PIC1_COMMAND_PORT, PIC_OCW3_READ_ISR);
    i686_outb(PIC2_COMMAND_PORT, PIC_OCW3_READ_ISR);
    return ((Uint16)i686_inb(PIC1_COMMAND_PORT)) | (((Uint16)i686_inb(PIC2_COMMAND_PORT)) << 8);
}

/*
 * Other functions
 */

Bool isPresent()
{
    picDisable();
    picSetMask(0x1337);
    return picGetMask() == 0x1337;
}
