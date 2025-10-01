;-----------------------------------------------------------------------------
; Assembly Directives
;
; .global - makes label visible to the linker
; .sect ".text" - 	tells the assembler to place the code into the section ".text",
;					which is a region of memory in the Flash (the area of memory
;					containing read-only executable instructions)
;
;-----------------------------------------------------------------------------
    .global getPsp
    .global setPsp
    .global getMsp
    .global setMsp


    .sect   ".text"
    .thumb

;-----------------------------------------------------------------------------
; Subroutines
;-----------------------------------------------------------------------------

getPsp:
    MRS R0, PSP         ; Move the value of the Process Stack Pointer into R0
    BX LR               ; Return to the caller

setPsp:
	MSR PSP, R0         ; Move the value of the Process Stack Pointer into R0
    BX LR               ; Return to the caller

getMsp:
    MRS R0, MSP         ; Move the value of the Main Stack Pointer into R0
    BX LR               ; Return to the caller

setMsp:
	MSR PSP, R0         ; Move the value of the Process Stack Pointer into R0
    BX LR               ; Return to the caller
