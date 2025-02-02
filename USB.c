/*
	USB
	
	Entry point for
	Microchip PIC18 USB Radio Panel firmware
	
	2024/07/15	Originated
	2024/10/26	Factored
	
 	References:
		[USB] Universal Serial Bus Specification, Revision 2.0
		[HID] Device Class Definition for Human Interface Devices (HID) Version 1.11
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#include <stdbool.h>

#include <xc.h>

#include "USB.h"
#include "USBEndpoint0.h"
#include "USBEndpoint1.h"



/*	Error
	Watch this compiler warning:
		advisory: (1510) non-reentrant function "_Error" appears in multiple call graphs and has been duplicated by the compiler
	According to the compiler user's guide, the duplication happens
		 "since it has been called from both main-line and interrupt code"
	While this was happening, the a debugger line break didn't work.
*/
void Error()
{
LATDbits.LATD3 = 0;
}


/*	USBInitialize
	Like this, we're also setting bits that have default values; as if we
	might call this ourselves; but we don't
*/
void USBInitialize()
{
UCFGbits.FSEN = 1;				// USB full speed
UCFGbits.UPUEN = 1;				// internal pull-up resistor enabled
UCFGbits.UTRDIS = 0;				// don't disable transceiver (default)
UCFGbits.PPB = 0;				// disable 'ping-pong' (double) buffering (default)

// ***** disable module and reset
// UCON = 0;

// UIEbits.URSTIE = 1;				// enable USB bus reset interrupts
UIEbits.URSTIE = 0;
UIEbits.TRNIE = 1;				// enable USB Transaction interrupts
UIEbits.IDLEIE = 1;				// enable USB Idle detection interrupts
UIEbits.ACTVIE = 0;

// reset USB device address *** needed?
UADDR = 0;

/* usb
ACTCON.ACTEN
*/

// enable the control endpoint
EnableEndpoint0();

/* the module needs to be fully preconfigured prior to setting this */
/* if the PLL is being used, it should be enabled at least 2 ms" */
do UCONbits.USBEN = 1; while (!UCONbits.USBEN);

// enable USB peripheral interrupts
PIE3bits.USBIE = 1;
}


/*	HandleUSBTransaction
	USB transaction completed
*/
static void HandleUSBTransaction()
{
// which Endpoint?
switch (USTATbits.ENDP) {
	// control endpoint?
	case 0:
		HandleUSBTransactionEndpoint0();
		break;
		
	// data endpoint?
	case 1:
		HandleUSBTransactionEndpoint1();
		break;
		
	default:
		Error();
	}
}


/*	USBInterruptService
	Service USB interrupts
*/
void USBInterruptService()
{
if (UIRbits.UERRIF)
	Error();

// idle?
if (UIEbits.IDLEIE && UIRbits.IDLEIF) {
	// suspend
	UCONbits.SUSPND = 1;

	// enable activity detection interrupt
	UIEbits.ACTVIE = 1;

	UIRbits.IDLEIF = 0;
	}

// activity?
if (UIEbits.ACTVIE && UIRbits.ACTVIF) {
	// awake
	UCONbits.SUSPND = 0;

	// disable activity detection interrupts
	UIEbits.ACTVIE = 0;

	// [PIC §24.5.1.1]
	do UIRbits.ACTVIF = 0; while (UIRbits.ACTVIF);
	}

// USB bus reset?
/* If a reset happens during suspend, then ACTVIF is set first*/
if (UIEbits.URSTIE && UIRbits.URSTIF) {
	Error();

	// *** flush existing transactions?
	while (UIRbits.TRNIF) UIRbits.TRNIF = 0;

	// [PIC §24.5.1] *** interrupt automatically clears UADDR

	// I think this is where I should set up EP0?
	UIRbits.URSTIF = 0;
	}

// while there are queued transactions
/* Loop over the USTAT FIFO here; otherwise, if another transaction has already
   completed, will reassert the interrupt within 6 instruction cycles.
   Theoretically it's an opportunity to avoid triggering another interrupt. */
while (UIRbits.TRNIF) {
	HandleUSBTransaction();

	// clear interrupt flag for this transaction
	UIRbits.TRNIF = 0;
	}
}
