/*
	Timer0
	
	Timer
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/27	Factored
	
 	References:
		[USB] Universal Serial Bus Specification, Revision 2.0
		[HID] Device Class Definition for Human Interface Devices (HID) Version 1.11
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#include <xc.h>

#include "Timer0.h"


/*	Timer0Initialize
	Initialize Timer 0
*/
void Timer0Initialize()
{
// enable timer
/* 8 MHz system clock; 2000 kHz instruction clock;
   with prescaler 2000 kHz / 256 = 7812.5 kHz timer clock */
/* We set this up initially for a 2ms timer for USB;
   afterwards, it becomes a 1 s timer to blink the LED */
T0CONbits.T08BIT = 0; // 16-bit timer
T0CONbits.T0CS = 0; // timer mode
T0CONbits.T0PS = 7; // 1:256
T0CONbits.PSA = 0; // prescaler enabled
// 7812.5 Hz timer clock: 2 ms = 2 timer periods
TMR0H = (uint8_t) (256 - 0);
TMR0L = (uint8_t) (256 - 2);
T0CONbits.TMR0ON = 1;
INTCONbits.TMR0IE = 1;				// note interrupts not globally enabled yet
INTCONbits.TMR0IF = 0;

// busy wait until 2ms is done
/* This sets the overflow and triggers the interrupt, which reconfigures the timer
   for the 1s blinking LED. */
while (TMR0L >= 256 - 2);

// now configure 1 s timer for blinking LED
TMR0H = (uint8_t) (256 - 1);
TMR0L = (uint8_t) (256 - 1);
}


/*	Timer0InterruptService
 
*/
void Timer0InterruptService()
{
LATDbits.LATD0 = !PORTDbits.RD0;

// 7812.5 Hz timer clock: 1s = 30 * 256 + 132 timer periods
TMR0H = 256 - 30;
TMR0L = 256 - 132;
}
