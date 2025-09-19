#include <stdint.h>

#ifndef STACK_HELPER_H_
#define STACK_HELPER_H_

// Returns the current value of the Process Stack Pointer (PSP)
extern uint32_t getPsp(void);

// Returns the current value of the Main Stack Pointer (MSP)
extern uint32_t getMsp(void);

#endif
