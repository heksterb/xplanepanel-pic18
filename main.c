/*
	main
	
	Entry point
	Microchip PIC18 USB Radio Panel firmware
	
	2024/07/15	Originated
	
	Organization:
	
	The source code files are organized around PIC peripheral application;
	so you could see PORTA referenced in more than once source file.
	
	There is no attempt of encapsulation of peripheral application.  So,
	for example, the SPI source code is aware of USB.
	
	
	Note throughout implementation-defined behavior [XC8 §11.10]:
	
	"The first bit-field defined in a structure is allocated the LSb position
	in the storage unit.  Subsequent bit-fields are allocated higher-order bits."
 
	The type chosen to represent an enumerated type depends on the enumerated values.
	A signed type is chosen if any value is negative; unsigned otherwise.
	If a char type is sufficient to hold the range of values, then this type is chosen;
	otherwise, an int type is chosen."
*/

// PIC18F45K50 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1L
#pragma config PLLSEL = PLL4X   // PLL Selection (4x clock multiplier)
#pragma config CFGPLLEN = OFF   // PLL Enable Configuration bit (PLL Disabled (firmware controlled))
#pragma config CPUDIV = CLKDIV6 // CPU System Clock Postscaler (CPU uses system clock divided by 6)
#pragma config LS48MHZ = SYS48X8// Low Speed USB mode with 48 MHz system clock (System clock at 48 MHz, USB clock divider is set to 8)

// CONFIG1H
#pragma config FOSC = INTOSCIO  // Oscillator Selection (Internal oscillator)
#pragma config PCLKEN = ON      // Primary Oscillator Shutdown (Primary oscillator enabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor (Fail-Safe Clock Monitor disabled)
#pragma config IESO = OFF       // Internal/External Oscillator Switchover (Oscillator Switchover mode disabled)

// CONFIG2L
#pragma config nPWRTEN = OFF    // Power-up Timer Enable (Power up timer disabled)
#pragma config BOREN = SBORDIS  // Brown-out Reset Enable (BOR enabled in hardware (SBOREN is ignored))
#pragma config BORV = 190       // Brown-out Reset Voltage (BOR set to 1.9V nominal)
#pragma config nLPBOR = OFF     // Low-Power Brown-out Reset (Low-Power Brown-out Reset disabled)

// CONFIG2H
#pragma config WDTEN = OFF      // Watchdog Timer Enable bits (WDT disabled in hardware (SWDTEN ignored))
#pragma config WDTPS = 32768    // Watchdog Timer Postscaler (1:32768)

// CONFIG3H
#pragma config CCP2MX = RC1     // CCP2 MUX bit (CCP2 input/output is multiplexed with RC1)
#pragma config PBADEN = OFF     // PORTB A/D Enable bit (PORTB<5:0> pins are configured as digital I/O on Reset)
#pragma config T3CMX = RC0      // Timer3 Clock Input MUX bit (T3CKI function is on RC0)
#pragma config SDOMX = RB3      // SDO Output MUX bit (SDO function is on RB3)
#pragma config MCLRE = ON       // Master Clear Reset Pin Enable (MCLR pin enabled; RE3 input disabled)

// CONFIG4L
#pragma config STVREN = ON      // Stack Full/Underflow Reset (Stack full/underflow will cause Reset)
#pragma config LVP = ON         // Single-Supply ICSP Enable bit (Single-Supply ICSP enabled if MCLRE is also 1)
#pragma config ICPRT = OFF      // Dedicated In-Circuit Debug/Programming Port Enable (ICPORT disabled)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Instruction set extension and Indexed Addressing mode disabled)

// CONFIG5L
#pragma config CP0 = OFF        // Block 0 Code Protect (Block 0 is not code-protected)
#pragma config CP1 = OFF        // Block 1 Code Protect (Block 1 is not code-protected)
#pragma config CP2 = OFF        // Block 2 Code Protect (Block 2 is not code-protected)
#pragma config CP3 = OFF        // Block 3 Code Protect (Block 3 is not code-protected)

// CONFIG5H
#pragma config CPB = OFF        // Boot Block Code Protect (Boot block is not code-protected)
#pragma config CPD = OFF        // Data EEPROM Code Protect (Data EEPROM is not code-protected)

// CONFIG6L
#pragma config WRT0 = OFF       // Block 0 Write Protect (Block 0 (0800-1FFFh) is not write-protected)
#pragma config WRT1 = OFF       // Block 1 Write Protect (Block 1 (2000-3FFFh) is not write-protected)
#pragma config WRT2 = OFF       // Block 2 Write Protect (Block 2 (04000-5FFFh) is not write-protected)
#pragma config WRT3 = OFF       // Block 3 Write Protect (Block 3 (06000-7FFFh) is not write-protected)

// CONFIG6H
#pragma config WRTC = OFF       // Configuration Registers Write Protect (Configuration registers (300000-3000FFh) are not write-protected)
#pragma config WRTB = OFF       // Boot Block Write Protect (Boot block (0000-7FFh) is not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protect (Data EEPROM is not write-protected)

// CONFIG7L
#pragma config EBTR0 = OFF      // Block 0 Table Read Protect (Block 0 is not protected from table reads executed in other blocks)
#pragma config EBTR1 = OFF      // Block 1 Table Read Protect (Block 1 is not protected from table reads executed in other blocks)
#pragma config EBTR2 = OFF      // Block 2 Table Read Protect (Block 2 is not protected from table reads executed in other blocks)
#pragma config EBTR3 = OFF      // Block 3 Table Read Protect (Block 3 is not protected from table reads executed in other blocks)

// CONFIG7H
#pragma config EBTRB = OFF      // Boot Block Table Read Protect (Boot block is not protected from table reads executed in other blocks)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>

#include "Display.h"
#include "LED.h"
#include "SPI.h"
#include "Switches.h"
#include "USB.h"
#include "Timer0.h"


/*	ISR
	Interrupt Service Routine

	Note throughout that flags may be set even without the corresponding interrupts
	being enabled
	
	Clear the condition flags immediately after making the decision to handle it;
	because the handling itself may trigger the condition again
*/
void __interrupt(high_priority) ISR(void)
{
// timer?
if (INTCONbits.TMR0IF) {
	// clear condition flag
	INTCONbits.TMR0IF = 0;
	
	Timer0InterruptService();
	}

// interrupt on change?
if (INTCONbits.IOCIF) {
	// clear condition flag
	INTCONbits.IOCIF = 0;
	
	SwitchesInterruptService();
	}

// controls?
if (INTCON3bits.INT2IF) {
	// clear condition flag (must be cleared by software)
	INTCON3bits.INT2IF = 0;
	
	ControlsServiceInterrupt();
	}

// SPI?
if (PIR1bits.SSPIF) {
	// clear condition flag *** ?
	PIR1bits.SSPIF = 0;
	
	// service interrupt
	SPIServiceInterrupt();
	}

// USB?
if (PIR3bits.USBIF) {
	// clear USB condition
	PIR3bits.USBIF = 0;
	
	// service interrupt
	USBInterruptService();
	}
}


/*	main
	Program entry point
*/
int main(void)
{
// INTCON.GIE is clear (interrupts disabled) at power-on reset
// RCONbits.IPEN is clear (priority levels disabled) at power-on reset

INTCONbits.INT0IF = 0;

// ***** remember Figure 2-1 for the hardware design

OSCCONbits.SCS = 0;			// primary clock (already done after reset)
OSCCONbits.IRCF = 7;			// 16 MHz internal oscillator
OSCCONbits.IDLEN = 1;			// enable Idle (as opposed to Sleep) modes

OSCTUNEbits.SPLLMULT = 1;		// PLL ×3
OSCCON2bits.PLLEN = 1;			// enable PLL multiplier

#if defined(__18F45K50)
	// disable individual resistors
	WPUB = 0;
	
	// enable pull-ups globally
	INTCON2bits.RBPU = 0;

#else
	#error define pull-up resistors
	#endif

// SPI
SPIInitialize();

// switches
SwitchesInitialize();

// LEDs
LEDInitialize();

// Timer 0
Timer0Initialize();

// USB
USBInitialize();

// enable peripheral interrupts (needed for USB and SPI)
INTCONbits.PEIE = 1;

// enable global interrupts
INTCONbits.GIE = 1;

for (;;) SLEEP();
}