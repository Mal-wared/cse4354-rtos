// Nicholas Nhat Tran
// 1002027150

// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

#include <stdbool.h>



//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void yield(void);
void reboot(void);
void ps(void);
void ipcs(void);
void kill(uint32_t pid);
void pkill(const char name[]);
void pi(bool on);
void preempt(bool on);
void sched(bool prio_on);
void pidof(const char name[]);
void run(const char name[]);
void shell(void);

#endif
