/*
	Display
	
	MAX6954 display driver and key scanner
	Microchip PIC18 USB Radio Panel firmware
	
	2024/11/01	Originated
	
 	References:
		[MAX] MAX6954 4-Wire Interfaced, 2.7V to 5.5V LED Display Driver
			with I/O Expander and Key Scan
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#pragma once


extern void DisplayInitialize(void);
extern void DisplayTerminate(void);
extern void ControlsServiceInterrupt(void);
extern void DisplayValues(__uint24, __uint24);
