// Nicholas Nhat Tran
// 1002027150

#ifndef STACK_HELPER_H_
#define STACK_HELPER_H_

#include <stdint.h>

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

extern void loadR3(uint32_t value);
extern void setPC(void);

extern uint32_t * saveContext(void);
extern void restoreContext(uint32_t * sp);

extern uint32_t * getPsp(void);
extern void setPsp(void * psp);

extern uint32_t * getMsp(void);
extern void setMsp(void * msp);

extern void setAspBit(void);
extern void setTMPL(void);

#endif
