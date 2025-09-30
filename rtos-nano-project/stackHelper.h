#include <stdint.h>

#ifndef STACK_HELPER_H_
#define STACK_HELPER_H_

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------
extern uint32_t getPsp(void);
extern void setPsp(uint32_t psp);

extern uint32_t getMsp(void);
extern void setMsp(uint32_t psp);

#endif
