/*
	LED
	
	PICDEM FS LEDs
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/27	Factored
	
 	References:
		[USB] Universal Serial Bus Specification, Revision 2.0
		[HID] Device Class Definition for Human Interface Devices (HID) Version 1.11
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#include <xc.h>

#include "LED.h"


/*	LEDInitialize
	Initialize LED handling
*/
void LEDInitialize()
{
LATD = 0;				// initialize zero output values

ANSELDbits.ANSD0 = 0;			// digital
ANSELDbits.ANSD1 = 0;			// digital
ANSELDbits.ANSD2 = 0;			// digital
ANSELDbits.ANSD3 = 0;			// digital

TRISDbits.TRISD0 = 0;			// output
TRISDbits.TRISD1 = 0;			// output
TRISDbits.TRISD2 = 0;			// output
TRISDbits.TRISD3 = 0;			// output

LATDbits.LATD0 = 1;
LATDbits.LATD1 = 1;
LATDbits.LATD2 = 1;
LATDbits.LATD3 = 1;
}
