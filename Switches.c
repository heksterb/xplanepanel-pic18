/*
	Switches
	
	PICDEM push-button switches
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/27	Factored
*/

#include <xc.h>

#include "Switches.h"


/*	SwitchesInitialize
	Initialize push-button switch handling
*/
void SwitchesInitialize()
{
// Port B switches
ANSELBbits.ANSB4 = 0;			// digital
ANSELBbits.ANSB5 = 0;			// digital

TRISBbits.RB4 = 1;			// input
TRISBbits.RB5 = 1;			// input

// enable interrupts
IOCBbits.IOCB4 = 1;			// interrupt-on-change RB4 enabled
IOCBbits.IOCB5 = 1;			// interrupt-on-change RB5 enabled
INTCONbits.IOCIE = 1;			// interrupt-on-change enabled
}


/*	SwitchesInterruptService
 
*/
void SwitchesInterruptService()
{
// set LEDs according to switch state
LATDbits.LATD2 = PORTBbits.RB4;
LATDbits.LATD3 = PORTBbits.RB5;
}
