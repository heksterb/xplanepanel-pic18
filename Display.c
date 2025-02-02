/*
	Display
	
	MAX6954 display driver and key scanner
	Microchip PIC18 USB Radio Panel firmware
	
	2024/11/01	Originated
	
 	References:
		[MAX] MAX6954 4-Wire Interfaced, 2.7V to 5.5V LED Display Driver
			with I/O Expander and Key Scan
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
	
	Note this comment from my Nordic version:
	
		Don't see the need for pull-up documented anywhere in [MAX6954],
		but it's reasonable (and P4 is always low otherwise).
	
	There is a very subtle reference in [MAX: GPIO and Key Scanning]
	
		These ports can be individually enabled as [...] open-drain logic outputs
	
	Also note the 4.7 k? pull-up in Figure 2 of
	
		https://www.analog.com/en/resources/design-notes/extending-max6954-and-max6955-keyscan-beyond-32-keys.html
*/

#include <xc.h>

#include "Display.h"
#include "SPI.h"
#include "USBEndpoint1.h"



enum Register {
	kRegisterNoOperation = 0x00,
	kRegisterDecodeMode = 0x01,
	kRegisterGlobalIntensity = 0x02,
	kRegisterScanLimit = 0x03,
	kRegisterConfiguration = 0x04,
	kRegisterPortConfiguration = 0x06,
	kRegisterTest = 0x07,
	kRegisterKeyAMaskDebounce = 0x08,
	kRegisterDigitTypeKeyAPressed = 0x0c,
	kRegisterDigit0Plane0 = 0x20,
	kRegisterDigit0APlane0 = 0x28
	};


union XX {
	char		i;
	struct {
		unsigned
				shutdownOff : 1,
				unused : 1,
				blinkFast : 1,
				blinkEnable : 1,
				blinkSync : 1,
				clearDigits : 1,
				intensityLocal : 1,
				blinkPhaseP0 : 1; // read-only?
		};
	};


/*	DisplayInitialize
	
*/
void DisplayInitialize()
{
static char buffer[] = {
	// scan limit 5 (digit pairs 0/0a through 5/5a)
	kRegisterScanLimit, 5,
	
	// global intensity
	kRegisterGlobalIntensity, 0,
	
	// digit type (all 7-segment displays)
	kRegisterDigitTypeKeyAPressed, 0x00,
	
	// decode mode (hexadecimal decoding)
	kRegisterDecodeMode, 0xFF,
	
	// configuration (shutdownOff)
	/* Trying to use compound literal here, but don't know how to convert that back to the integral type*/
	kRegisterConfiguration, 0x01,
	
	// port configuration (8 keys scanned; P1,2,3 are left as output; P4 becomes IRQ)
	kRegisterPortConfiguration, 0x20,
	
	// key mask (enable interrupt on 0)
	kRegisterKeyAMaskDebounce + 0, 1 << 0,
	
	// read Key A Debounce register to reset IRQ
	0x80 | (kRegisterKeyAMaskDebounce + 0), 0,
	
	// test pattern
	#if 0
		0x20, 0x80 | 8,
		0x21, 0x80 | 8,
		0x22, 0x80 | 8,
		0x23, 0x80 | 8,
		0x24, 0x80 | 8,
		0x25, 0x80 | 8,
		
		0x28, 0x80 | 8,
		0x29, 0x80 | 8,
		0x2A, 0x80 | 8,
		0x2B, 0x80 | 8,
		0x2C, 0x80 | 8,
		0x2D, 0x80 | 8,
	
	#endif
	};

// transfer MAX 6954 configuration
SPIStartExchange(buffer, sizeof buffer, NULL);

/* Only enable this after we've intialized the MAX so that we know it will be
   able to process and responsive to SPI. */

// interrupt on falling edge (MAX6954 IRQ output is active low)
INTCON2bits.INTEDG2 = 0;

// clear condition flag
INTCON3bits.INT2IF = 0;

// configure port B2 as digital input INT2
ANSELBbits.ANSB2 = 0;			// digital
TRISBbits.RB2 = 1;			// input
WPUBbits.WPUB2 = 1;			// enable pull-up

// enable INT2 external interrupt
INTCON3bits.INT2IE = 1;
}


/*	DisplayTerminate
 
 */
void DisplayTerminate()
{
// disable INT2 external interrupt
INTCON3bits.INT2IE = 0;

// clear condition flag (just in case)
INTCON3bits.INT2IF = 0;

// deconfigure display driver
static char buffer[] = {
	// configuration (shutdownOn)
	/* Trying to use compound literal here, but don't know how to convert that back to the integral type*/
	kRegisterConfiguration, 0x00,
	
	// key mask (disable interrupts)
	kRegisterKeyAMaskDebounce + 0, 0
	};

// transfer MAX 6954 configuration
SPIStartExchange(buffer, sizeof buffer, NULL);
}



__uint24 gValue0, gValue1;


/*	DisplayValues
	Cause the given values to be displayed
*/
void DisplayValues(
	__uint24	v0,
	__uint24	v1
	)
{
gValue0 = v0;
gValue1 = v1;

static char buffer[24];
buffer[ 0] = kRegisterDigit0Plane0 + 5;
buffer[ 1] = v0 % 10; v0 /= 10;
buffer[ 2] = kRegisterDigit0Plane0 + 4;
buffer[ 3] = v0 % 10; v0 /= 10;
buffer[ 4] = kRegisterDigit0Plane0 + 3;
buffer[ 5] = v0 % 10; v0 /= 10;
buffer[ 6] = kRegisterDigit0Plane0 + 2;
buffer[ 7] = (v0 % 10) | 0x80; v0 /= 10;	// with decimal point
buffer[ 8] = kRegisterDigit0Plane0 + 1;
buffer[ 9] = v0 % 10; v0 /= 10;
buffer[10] = kRegisterDigit0Plane0 + 0;
buffer[11] = v0 % 10;

buffer[12] = kRegisterDigit0APlane0 + 5;
buffer[13] = v1 % 10; v1 /= 10;
buffer[14] = kRegisterDigit0APlane0 + 4;
buffer[15] = v1 % 10; v1 /= 10;
buffer[16] = kRegisterDigit0APlane0 + 3;
buffer[17] = v1 % 10; v1 /= 10;
buffer[18] = kRegisterDigit0APlane0 + 2;
buffer[19] = (v1 % 10) | 0x80; v1 /= 10;	// with decimal point
buffer[20] = kRegisterDigit0APlane0 + 1;
buffer[21] = v1 % 10; v1 /= 10;
buffer[22] = kRegisterDigit0APlane0 + 0;
buffer[23] = v1 % 10;

// send SPI commands to MAX 6954 to display
SPIStartExchange(buffer, sizeof buffer, NULL);
}


/*	gReadKeyADebounced
	Before SPI exchange: the command to read the Key A Debounced register;
	after the exchange completed, the value of that register
*/
static char gReadKeyADebounced[2];


/*	ReadDebouncedKeyA
 
*/
static void ReadDebouncedKeyA()
{
// *** if key pressed

// swap the two displayed values
DisplayValues(gValue1, gValue0);

// send back to host
SendValues(gValue0, gValue1);
}


/*	ControlsServiceInterrupt
	Respond to changes in button controls handled by the MAX
	
	What I'm observing is that IRQ does remain low unless the debounced register
	is read, but that IRQ is low for at least half a second, regardless.  Is that
	the effect of the key debouncing?  Doesn't make sense.
*/
void ControlsServiceInterrupt()
{
// read Key A debounced
gReadKeyADebounced[0] = 0x80 | (kRegisterKeyAMaskDebounce + 0);
gReadKeyADebounced[1] = 0 /* dummy */;

// transfer MAX 6954 read commands
SPIStartExchange(gReadKeyADebounced, sizeof gReadKeyADebounced, ReadDebouncedKeyA);
}

