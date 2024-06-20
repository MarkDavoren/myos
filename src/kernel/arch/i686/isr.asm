[bits 32]

%macro MAKE_ISRS 1-*
  %assign numVectors 256
  %assign ii 0
  %rep numVectors
    %defstr sii ii
    %strcat svar 'i686_ISR_', sii
    %deftok var svar
    global var
    var:
    %ifidn ii,%1
      %rotate 1
    %else
      push 0
    %endif
    push ii
    jmp isr_common

    %assign ii ii+1
  %endrep

  %assign ii 0
  global i686_isrTable
  i686_isrTable:
  %rep numVectors
    %defstr sii ii
    %strcat svar 'i686_ISR_', sii
    %deftok var svar
    dd var
    %assign ii ii+1
  %endrep

%endmacro

;
; Create an ISR for each possible interrupt and exception
; They all push the interrupt number to the stack and jump to isr_common
; For some exceptions, the CPU will push an error code first, for others we push 0
; so isr_common can safely assume there are two parameters on the stack
;

MAKE_ISRS 8, 10, 11, 12, 13, 14, 17, 21

;
; When an interrupt or exception occurs, the processor jumps to the segment and offset specified in
; the corresponding entry in the IDT. We set up the IDT such that the nth IDT entry points to i686_ISR_n
; which pushes n onto the stack and jumps here.
;
; Prior to jumping through the IDT, the processor pushes:
;   SS, ESP, EFLAGS, CS, EIP, and sometimes an error code (if it doesn't then the i686_ISR_n will push zero)
;
; At the point when isrHandler is called the stack looks like
;
;   SS          - If there was a change in CPL
;   ESP         - If there was a change in CPL
;   EFLAGS
;   CS
;   EIP
;   Error code (or zero)
;   Interrupt number
;   EAX
;   ECX
;   EDX
;   EBX
;   ESP
;   EBP
;   ESI
;   EDI
;   DS      <--\
;   pointer to /
;

extern isrHandler
global isr_common
isr_common:
    pusha               ; pushes in order: eax, ecx, edx, ebx, esp, ebp, esi, edi

    xor eax, eax        ; push ds
    mov ax, ds
    push eax

    ; The processor has loaded the CS segment from the IDT, but the none of the others have been set
    mov ax, 0x10        ; use kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp            ; pass pointer to stack to C, so we can access all the pushed information
    call isrHandler
    add esp, 4

    pop eax             ; restore old segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                ; pop what we pushed with pusha
    add esp, 8          ; remove error code and interrupt number
    iret                ; will pop: cs, eip, eflags, ss, esp