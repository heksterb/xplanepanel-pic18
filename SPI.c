/*
	SPI
	
	Serial Peripheral Interface
	Microchip PIC18 USB Radio Panel firmware
	
	2024/10/27	Originated
	
 	References:
		[MAX] MAX6954 4-Wire Interfaced, 2.7V to 5.5V LED Display Driver
			with I/O Expander and Key Scan
		[PIC] Microchip PIC18(L)F2X/45K50 Data Sheet
*/

#include <xc.h>

#include "SPI.h"


extern void Error(void);


/*	SPIInitialize
	Initialize Serial Peripheral Interface
	
	NOTE: Pay attention to the configuration bits to verify the pin assignments
	(i.e., SDOMX for the PIC18F45K50)
*/
void SPIInitialize()
{
SSP1STAT = 0;
SSP1CON1 = 0;

// SPI master Fosc / 4
/* 8 MHz system clock ÷ 4 = 2MHz SCK ? 500 ns clock period;
   this is much greater than MAX6954 minimum clock period 38.4 ns */
SSP1CON1bits.SSPM = 0;

// clock idle low
SSP1CON1bits.CKP = 0;

// data on clock rising edge
/* [MAX] "DIN must be stable when sampled on the rising edge of CLK" */
SSP1STATbits.CKE = 1;

/* [MAX] "DOUT is stable on the rising edge of CLK" */
SSP1STATbits.SMP = 0;

// SDI
ANSELBbits.ANSB0 = 0;			// digital
TRISBbits.RB0 = 1;			// input

// SDO
TRISBbits.RB3 = 0;			// output

// SCK master
TRISBbits.RB1 = 0;			// output

// SPI slave CS
/* Note that this happens to also be SS* for when the PIC itself is operating
   as slave.  We're not, but it seems appropriate to use this to *drive* CS
   as master. */
LATAbits.LATA5 = 1;			// high is deselect
TRISAbits.RA5 = 0;			// output

// enable interrupts
PIE1bits.SSPIE = 1;

// enable module
SSP1CON1bits.SSPEN = 1;
}


/*	gSPIData
	Data to send
*/
static void (*gSPICallback)();
static char *gSPIData;
static uint8_t gSPIDataL;


/*	SPIServiceInterrupt
	Byte was sent
*/
void SPIServiceInterrupt()
{
// store exchanged data in buffer
*gSPIData++ = SSP1BUF, --gSPIDataL;

// end of two-byte MAX command?
if (gSPIDataL % 2 == 0)
	// disable CS
	LATAbits.LATA5 = 1;

// still more data to exchange?
if (gSPIDataL) {
	// will start a new two-byte MAX command?
	if (gSPIDataL % 2 == 0)
		// enable CS
		LATAbits.LATA5 = 0;
	
	// send next byte
	SSP1BUF = *gSPIData;
	}

// buffer exchange completed
else {
	// no more data to send
	gSPIData = NULL;
	
	// notify caller
	if (gSPICallback) {
		// clear global state so the callback can schedule another transfer
		void (*callback)(void) = gSPICallback;
		gSPICallback = NULL;
		
		// call back
		(*callback)();
		}
	
	}
}


/*	SPIStartExchange
	SPI fundamentally rotates bytes from the master into a chain of slaves;
	The data in the given array is pushed out; data that arrives back is
	stored back and replaces the original data in the array
	
	***** if no callback, then also no writeback?
*/
void SPIStartExchange(
	char		*data,
	uint8_t		dataL,
	void		(*callback)()
	)
{
// should not be an exchange in progress already
/* This can happen when we're servicing an interrupt for a keypress and sending
   SPI commands to clear interrupt; and at the same time receiving a output report
   from USB.  I think the odds of this happening are minuscule; but it would be nice
   if we handled it properly (at least sending STALL back to USB). */
if (gSPIData) { Error(); return; }

// we're not optimizing for the special case of a zero-length exchange
if (dataL == 0) Error();

// any previously received data should already have been removed
if (SSP1STATbits.BF) Error();

// data to exchange
gSPICallback = callback;
gSPIData = data;
gSPIDataL = dataL;

// enable SPI slave Chip Select
LATAbits.LATA5 = 0;

// send first byte
SSP1BUF = *gSPIData;
}