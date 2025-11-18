;-----------------------------------------------------------------------------
; Assembly Directives
;
; .global - makes label visible to the linker
; .sect ".text" - 	tells the assembler to place the code into the section ".text",
;					which is a region of memory in the Flash (the area of memory
;					containing read-only executable instructions)
;
;-----------------------------------------------------------------------------
	.global loadR3
	.global setSP
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

loadR3:
    MOV R3, R0      	; Test value: should put a value into R3
    ISB
    BX LR

setSP:
	BX R3

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


