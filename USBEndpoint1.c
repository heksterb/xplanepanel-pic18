/*
	USBEndpoint1
	
	USB HID endpoint
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/26	Factored
	
 	References:
		[USB] Universal Serial Bus Specification, Revision 2.0
		[HID] Device Class Definition for Human Interface Devices (HID) Version 1.11
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#include <xc.h>

#include "Display.h"
#include "SPI.h"
#include "USB.h"
#include "USBEndpoint1.h"


/*	gToggleIN
	
	[USB §8.5.4] "interrupt endpoint is initialized to the DATA0 PID by any configuration event"
*/
static char gToggleIN;


static void ArmEndpoint1OUT()
{
if (ep1Out.STAT.UOWN) Error();

// data to expect in the next OUT transaction
ep1Out.ADR = ep1OutBuffer;
ep1Out.CNT = sizeof ep1OutBuffer;

ep1Out.STAT.i = 0;

// 'arm' Endpoint 1 OUT in anticipation of next Data Stage Transaction
ep1Out.STAT.UOWN = 1;				// must be separate instruction
}


static void ArmEndpoint1IN()
{
if (ep1In.STAT.UOWN) Error();

// data to send in the next IN transaction
/* I *think* that if you send more data back than the host expects (even from the HID descriptor?!)
   then the transaction fails (possibly stalls) and you never get the TRNIF. */
ep1In.ADR = ep1InBuffer;
ep1In.CNT = sizeof ep1InBuffer;
ep1In.STAT.i = 0;
ep1In.STAT.DTS = gToggleIN;
ep1In.STAT.DTSEN = 1;

// 'arm' Endpoint 1 IN in anticipation of next Data Stage Transaction
ep1In.STAT.UOWN = 1;				// must be separate instruction
}


/*	EnableEndpoint1
	Enable the HID data endpoint
*/
void EnableEndpoint1()
{
ep1Out.STAT.i = 0;
ep1In.STAT.i = 0;

// be prepared for host to send report
ArmEndpoint1OUT();

// do not arm IN until we cause a change in values *** respect SetIdle though

UEP1bits.EPHSHK = 1;				// enable USB handshake
UEP1bits.EPCONDIS = 1;				// disable Control
UEP1bits.EPOUTEN = 1;				// enable OUT transactions
UEP1bits.EPINEN = 1;				// enable IN transactions

// enable display driver and key scanner
DisplayInitialize();
}


/*	DisableEndpoint1
	Disable the HID data endpoint
*/
void DisableEndpoint1()
{
// disable display
DisplayTerminate();

// disarm Endpoint 1 OUT
ep1Out.STAT.UOWN = 0;

// disarm Endpoint 1 IN
ep1In.STAT.UOWN = 0;

// disable Endpoint 1 transactions *****
UEP1 = 0;
}


typedef union {
	struct {
		__uint24	v0;
		uint16_t	i0;
		};
	struct {
		uint16_t	i1;
		__uint24	v1;
		};
	char		b[5];
	} Report;



/*	HandleEndpoint1OUT
	Receive HID report for display
*/
static void HandleEndpoint1OUT()
{
// copy the HID report (seems to be more code-efficient than pointer-aliasing)
Report r;
r.b[0] = ep1OutBuffer[0];
r.b[1] = ep1OutBuffer[1];
r.b[2] = ep1OutBuffer[2];
r.b[3] = ep1OutBuffer[3];
r.b[4] = ep1OutBuffer[4];

// extract the 20-bit values *** assembly
__uint24 v0 = 0, v1 = 0;
v0 = r.v0 & 0x0FFFFF;
v1 = r.v1 >> 4;

// display the values
DisplayValues(v0, v1);

// wait for new OUT transfers
ArmEndpoint1OUT();
}


static void HandleEndpoint1IN()
{
/* Do nothing; this will cause subsequent INs to return NACK [***].  We'll arm
   the endpoint once there is new data.*/
// *** respect SetIdle

// prepare the data toggle for a next IN transaction
/* [USB §8.5.4
	When an endpoint is using the interrupt transfer mechanism
	for actual interrupt data, the data toggle protocol must be followed. "
*/
gToggleIN = !gToggleIN;
}


/*	SendValues
	Send updated values to the host
*/
void SendValues(
	__uint24	value0,
	__uint24	value1
	)
{
// construct a report from the two 20-bit values *** assembly
Report r;
r.v0 = value0;
r.v1 = value1 << 4;
r.b[2] = (value0 >> 16) | ((value1 & 0x0F) << 4);

ep1InBuffer[0] = r.b[0];
ep1InBuffer[1] = r.b[1];
ep1InBuffer[2] = r.b[2];
ep1InBuffer[3] = r.b[3];
ep1InBuffer[4] = r.b[4];

// send report on next IN transaction
ArmEndpoint1IN();
}


/*	HandleUSBTransactionEndpoint1

*/
extern void HandleUSBTransactionEndpoint1()
{
if (USTATbits.DIR == 0)
	HandleEndpoint1OUT();

else
	HandleEndpoint1IN();
}