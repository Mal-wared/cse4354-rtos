; Make the function names visible to the linker
    .global getPsp
    .global getMsp

; Define the code section as ".text"
    .sect   ".text"
    .thumb



getPsp:
    MRS R0, PSP         ; Move the value of the Process Stack Pointer into R0
    BX LR               ; Return to the caller

getMsp:
    MRS R0, MSP         ; Move the value of the Main Stack Pointer into R0
    BX LR               ; Return to the caller
