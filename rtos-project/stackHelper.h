#ifndef STACK_HELPER_H_
#define STACK_HELPER_H_

#include <stdint.h>

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

extern void putSomethingIntoR3(void);
extern uint32_t * getPsp(void);
extern void setPsp(void * psp);

extern uint32_t * getMsp(void);
extern void setMsp(void * msp);

extern void setAspBit(void);
extern void setTMPL(void);

#endif
