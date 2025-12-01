// Tasks
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef TASKS_H_
#define TASKS_H_

#define BLUE_LED   PORTF,2 // on-board blue LED
#define RED_LED    PORTA,2 // off-board red LED
#define ORANGE_LED PORTA,3 // off-board orange LED
#define YELLOW_LED PORTA,4 // off-board yellow LED
#define GREEN_LED  PORTE,0 // off-board green LED

#define BUTTON1    PORTA,5 // off-board pushbutton
#define BUTTON2    PORTA,6 // off-board pushbutton
#define BUTTON3    PORTA,7 // off-board pushbutton
#define BUTTON4    PORTE,3 // off-board pushbutton
#define BUTTON5    PORTE,2 // off-board pushbutton
#define BUTTON6    PORTE,1 // off-board pushbutton

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initHw(void);

void idle(void);
void flash4Hz(void);
void oneshot(void);
void partOfLengthyFn(void);
void lengthyFn(void);
void readKeys(void);
void debounce(void);
void uncooperative(void);
void errant(void);
void important(void);
void highPrioHog(void);
void highPrioHog2(void);
void testPiLow(void);
void testPiMedium(void);
void testPiHigh(void);

#endif
