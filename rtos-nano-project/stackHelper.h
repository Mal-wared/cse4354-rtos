#ifndef STACK_HELPER_H_
#define STACK_HELPER_H_

#include <stdint.h>

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------
extern uint32_t * getPsp(void);
extern void setPsp(void * psp);

extern uint32_t * getMsp(void);
extern void setMsp(void * msp);

#endif
