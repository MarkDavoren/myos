ENTRY(start)
OUTPUT_FORMAT("binary")
phys = 0x100000;

SECTIONS
{
    . = phys;

    .entry              : { __entry_start = .;      *(.entry)   }   /* stage 2 entry code */
    .text               : { __text_start = .;       *(.text)    }   /* executable code */
    .data               : { __data_start = .;       *(.data)    }   /* initialized global data */
    .rodata             : { __rodata_start = .;     *(.rodata)  }   /* readonly data - consts and strings */
    .bss                : { __bss_start = .;        *(.bss)     }   /* unitialized global data */

    __bss_end = .;
    __end = .;
}