;-----------------------------------------------------------------------------
; Assembly Directives
;
; .global - makes label visible to the linker
; .sect ".text" - 	tells the assembler to place the code into the section ".text",
;					which is a region of memory in the Flash (the area of memory
;					containing read-only executable instructions)
;
;-----------------------------------------------------------------------------
	.global putSomethingIntoR0
    .global getPsp
    .global setPsp
    .global getMsp
    .global setMsp
    .global setAspBit
    .global setTMPL

    .sect   ".text"
    .thumb

;-----------------------------------------------------------------------------
; Subroutines
;-----------------------------------------------------------------------------

putSomethingIntoR0:
    MOV R3, #1      	; Move the value of the Process Stack Pointer into R0
    BX LR               ; Return to the caller

getPsp:
    MRS R0, PSP         ; Move the value of the Process Stack Pointer into R0
    ISB
    BX LR               ; Return to the caller

setPsp:
	MSR PSP, R0         ; Move the value of the Process Stack Pointer into R0
	ISB
    BX LR               ; Return to the caller

getMsp:
    MRS R0, MSP         ; Move the value of the Main Stack Pointer into R0
    ISB
    BX LR               ; Return to the caller

setMsp:
	MSR MSP, R0         ; Move the value of the Process Stack Pointer into R0
	ISB
    BX LR               ; Return to the caller

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
