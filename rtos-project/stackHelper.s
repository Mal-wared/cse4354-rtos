;-----------------------------------------------------------------------------
; Assembly Directives
;
; .global - makes label visible to the linker
; .sect ".text" - 	tells the assembler to place the code into the section ".text",
;					which is a region of memory in the Flash (the area of memory
;					containing read-only executable instructions)
;
;-----------------------------------------------------------------------------
	.global putSomethingIntoR3
    .global getPsp
    .global setPsp
    .global getMsp
    .global setMsp
    .global setAspBit
    .global setTMPL
    .global launchFirstTask

    .sect   ".text"
    .thumb

;-----------------------------------------------------------------------------
; Subroutines
;-----------------------------------------------------------------------------

putSomethingIntoR3:
    MOV R3, #103      	; Test value: should put 0x67 into R3
    BX LR

getPsp:
    MRS R0, PSP         ; Move the value of the Process Stack Pointer into R0
    ISB
    BX LR

setPsp:
	MSR PSP, R0         ; Move the value of the Process Stack Pointer into R0
	ISB
    BX LR

getMsp:
    MRS R0, MSP         ; Move the value of the Main Stack Pointer into R0
    ISB
    BX LR

setMsp:
	MSR MSP, R0         ; Move the value of the Main Stack Pointer into R0
	ISB
    BX LR

; page 89
setAspBit:
	MRS R0, CONTROL		; Read CONTROL register into R0
	ORR R0, R0, #2		; R0 | #2 - using bitwise OR to not overwrite CONTROL bits
	MSR CONTROL, R0		; Write newly modified R0 into CONTROL
	ISB
	BX LR

setTMPL:
	MRS R0, CONTROL
	ORR	R0, R0, #1
	MSR CONTROL, R0
	BX LR

launchFirstTask:
    MSR PSP, R0       ; Set the Process Stack Pointer (PSP) from R0
    POP {R4-R11}      ; Restore software-saved registers from the new stack
    			      ; LDR LR, =0xFFFFFFFD ; Load the EXC_RETURN value (Thread mode, PSP stack)
    MOVW LR, #0xFFFD  ; Load lower 16 bits (0x0000FFFD)
	MOVT LR, #0xFFFF  ; Load upper 16 bits (0xFFFF....), LR is now 0xFFFFFFFD
    BX LR             ; Branch to the task

    ; We should never get here, but it's good practice
    NOP

    ; Mark the end of the file
END
