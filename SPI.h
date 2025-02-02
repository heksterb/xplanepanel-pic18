/*
	SPI
	
	Serial Peripheral Interface
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/27	Originated
	
 	References:
		[USB] Universal Serial Bus Specification, Revision 2.0
		[HID] Device Class Definition for Human Interface Devices (HID) Version 1.11
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#pragma once


extern void SPIInitialize(void);
extern void SPIServiceInterrupt(void);
extern void SPIStartExchange(char *data, uint8_t dataL, void (*)());
