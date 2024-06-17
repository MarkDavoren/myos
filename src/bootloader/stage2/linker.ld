ENTRY(entry)
OUTPUT_FORMAT("binary")
phys = 0x00000500;          /* Start everything at 0x500 (after IVT plus some space) */

SECTIONS
{
    . = phys;

    .entry              : { __entry_start = .;      *(.entry)   }   /* stage 2 entry code */
    .text               : { __text_start = .;       *(.text)    }   /* executable code */
    .data               : { __data_start = .;       *(.data)    }   /* initialized global data */
    .rodata             : { __rodata_start = .;     *(.rodata)  }   /* readonly data - consts and strings */
    .bss                : { __bss_start = .;        *(.bss)     }   /* unitialized global data */
    
    __end = .;
}