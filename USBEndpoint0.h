/*
	USBEndpoint0
	
	USB control endpoint
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/26	Factored
	
 	References:
		[USB] Universal Serial Bus Specification, Revision 2.0
		[HID] Device Class Definition for Human Interface Devices (HID) Version 1.11
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#pragma once


extern void DisableEndpoint0(void);
extern void EnableEndpoint0(void);
extern void HandleUSBTransactionEndpoint0(void);
