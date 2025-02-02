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

#include <stdbool.h>

#include <xc.h>

#include "USB.h"
#include "USBEndpoint1.h"


/*

	descriptors
 
*/

enum { kEndpoint0MaximumPacketLength = 32 };

static const DeviceDescriptor gDeviceDescriptor = {
	sizeof gDeviceDescriptor,
	kDevice,
	0x0200, // USB version 02.00
	0x00, // [DCDHID §5.1] class type is not defined at the device descriptor but at the interface descriptor
	0x00,				// subclass: should not be used [HID §5.1]
	0x00,				// protocol: should not be used [HID §5.1]
	kEndpoint0MaximumPacketLength,	// maximum packet size for Endpoint 0
	0xF055, // vendor ID *** (pseudo-officially like "FOSS")
	0x1234, // product ID ***
	0x0001, // device version 00.01
	1, // manufacturer descriptor
	2, // product descriptor
	0, // no serial number descriptor
	1 // number of configurations ***
	};


/*	gReportDescriptor
	HID report descriptor for the panel
*/
static const struct {
	// [HID] Usage 0x01 to 0x20 are for top-level collections
	/* I don't see an alternative to using a 'vendor' page: there doesn't seem to be any 'LC' (linear control)
	   that we can use. */
	HIDReportDescriptorItem16 usagePage;
	HIDReportDescriptorItem8 usage;
	HIDReportDescriptorItem8 beginCollectionApplication;
	
	HIDReportDescriptorItem8 logicalMinimumInput;
	HIDReportDescriptorItem32 logicalMaximumInput;
	HIDReportDescriptorItem8 reportCountInput;
	HIDReportDescriptorItem8 reportSizeInput;
	
	HIDReportDescriptorItem8 usageInput;
	HIDReportDescriptorItem8 input;
	
	HIDReportDescriptorItem8 usageOutput;
	HIDReportDescriptorItem8 output;
	
	HIDReportDescriptorItem0 endCollectionApplication;
	} gReportDescriptor = {
	{ { 2, kGlobal, kUsageGlobal }, 0xffa0 },			// Usage Page is high 16 bits of Usage ID
	
	{ { 1, kLocal, kUsageLocal }, 0x01 },
	{ { 1, kMain, kCollection }, kCollectionApplication },
	
	{ { 1, kGlobal, kLogicalMinimum }, 0 },
	{ { 3, kGlobal, kLogicalMaximum }, 999999 },
	{ { 1, kGlobal, kReportCount }, 2 /* displays */ },
	{ { 1, kGlobal, kReportSize }, 20 /* bits */ },
	
	{ { 1, kLocal, kUsageLocal }, 0x21 },
	{ { 1, kMain, kInput }, 0b10100010 },
	
	{ { 1, kLocal, kUsageLocal }, 0x22 },
	{ { 1, kMain, kOutput }, 0b10100010 },
	
	{ { 0, kMain, kCollectionEnd } }
	};


/* Currently, our device operates in a way that has the behavior of a radio frequency
   panel: it swaps active/standby frequencies and allows them to be adjusted with
   controls.  It could be said that the 'source of truth' resides with our device.
   
   I can imagine a different mode of operation where the device operates simply
   as a human interface with no associated built-in behavior.  Maybe this would best
   be exposed as a different USB Configuration. */
enum { kConfigurationRadioPanel = 1 };


/* [HID §7.1]
	When a GetDescriptor(Configuration) request is issued, it returns
		the Configuration descriptor,
		all Interface descriptors,
		all Endpoint descriptors, and
		the HID descriptor for each interface.
	[...] The HID descriptor shall be interleaved between
	the Interface and Endpoint descriptors for HID Interfaces. */
static const struct {
	ConfigurationDescriptor configuration;
	InterfaceDescriptor interface;
	HIDClassDescriptor1 hid;
	EndpointDescriptor endpoints[2];
	} gConfigurationDescriptor = {
	/* configuration */ {
		sizeof gConfigurationDescriptor.configuration,
		kConfiguration,
		sizeof gConfigurationDescriptor,
		1, // number of interfaces
		kConfigurationRadioPanel,	// configuration value
		0, // no string descriptor
		0, // reserved 0
		false, // no remote wake-up
		false, // self-powered
		1, // reserved1 (set to 1)
		40 / 2 // maximum power (in 2mA units)
		},
	
	/* interface */ {
		sizeof gConfigurationDescriptor.interface,
		kInterface,
		0, // index
		0, // alternate setting
		2, // number of endpoints
		kInterfaceClassHID,
		0x00,				// subclass: not a Boot Device [HID §4.2]
		0x00,				// protocol: not a Boot Device [HID §4.3]
		0 // no string descriptor
		},
	
	/* HID class descriptor [HID §6.2.1] */ {
		sizeof gConfigurationDescriptor.hid,
		kHID,
		0x0111,				// class specification version: 01.11
		0,				// country code: no localization
		1,				// number of descriptors
		{ kHIDReport, sizeof gReportDescriptor }
		},
	
	/* endpoints */ {
		/* [0] */ {
			sizeof gConfigurationDescriptor.endpoints[0],
			kEndpoint,
			1, // endpoint number
			0, // reserved
			kOUT, // direction
			kInterrupt, // transfer
			kNoSynchronization, // not an isochronous endpoint
			kData, // usage
			0, // reserved
			5,
			100 /* polling interval *** */
			},
		
		/* [1] */ {
			sizeof gConfigurationDescriptor.endpoints[1],
			kEndpoint,
			1, // endpoint number
			0,
			kIN,
			kInterrupt,
			kNoSynchronization, // not an isochronous endpoint
			kData,
			0,
			5,
			100 /* polling interval *** */
			}
		}
	};



/*	EnableEndpoint0
	Enable the control endpoint
*/
void EnableEndpoint0()
{
// arm Endpoint 0 OUT in anticipation of the Setup Transaction
ep0Out.ADR = ep0OutBuffer;
ep0Out.CNT = sizeof ep0OutBuffer;
ep0Out.STAT.i = 0;
// ep0Out.Stat.DTS = 0;				// data 0 packet expected next
ep0Out.STAT.DTSEN = 1;
ep0Out.STAT.UOWN = 1;				// must be separate instruction

// disarm Endpoint 1 IN
ep0In.STAT.i = 0;

UEP0bits.EPHSHK = 1;				// enable USB handshake
UEP0bits.EPCONDIS = 0;				// don't disable Control operations
UEP0bits.EPOUTEN = 1;				// enable OUT transactions
UEP0bits.EPINEN = 1;				// enable IN transactions
}


/*	DisableEndpoint0
	Disable the control endpoint
*/
void DisableEndpoint0()
{
// disable Endpoint 0 transactions *****
UEP0 = 0;

// disarm Endpoint 0 OUT
ep0Out.STAT.UOWN = 0;

// disarm Endpoint 0 IN
ep0In.STAT.UOWN = 0;
}


/*	ArmEndpoint0INStatus
	Prepare Endpoint 0 IN to send a Status
*/
static void ArmEndpoint0INStatus()
{
// debugging: expect that we own this now
if (ep0In.STAT.UOWN) Error();

// sending ZLP results in ACK (*** ?)
ep0In.STAT.i = 0;
ep0In.ADR = ep0InBuffer;
ep0In.CNT = 0;
ep0In.STAT.DTS = 1;				// Data 1 packet expected next [USB ***]
ep0In.STAT.DTSEN = 1;
ep0In.STAT.UOWN = 1;				// must be separate instruction
}


/*	gEndpoint0OUT
	If Data is not NULL, then we are in the Data or Status Stage of a Control Write Transfer
	If DataL is not zero, then we are in the Data
*/
static char *gEndpoint0OUTData;
static char gEndpoint0OUTDataL;
static bool gEndpoint0OUTToggle;


static const char *gEndpoint0INData;
static char gEndpoint0INDataL;
static bool gEndpoint0INToggle;


/*	ArmEndpoint0OUT
	Prepare Endpoint 0 OUT
	
	We expect an OUT in one of three cases:
	
	1)	as a DATA0 Setup Stage Transaction;
		this can happen at any time, even if a Control Read or Write is
		still in progress [USB §8.5.3]
	2)	as a DATA0/1 during the Data Stage of a Control Write;
	3)	as a DATA1 the Status Stage of a Control Read
*/
static void ArmEndpoint0OUT()
{
if (ep0Out.STAT.UOWN) Error();

ep0Out.ADR = ep0OutBuffer;
ep0Out.CNT = sizeof ep0OutBuffer;
ep0Out.STAT.i = 0;

if (
	// in a Control Write Transer?
	gEndpoint0OUTData &&
		// still expecting data?
		gEndpoint0OUTDataL > 0 &&
		
		// next Transaction is DATA1?
		gEndpoint0OUTToggle == 1 ||
	
	// in a Control Read Transfer?
	gEndpoint0INData &&
		// not expecting data, but Status
		gEndpoint0INDataL == 0
	)
	// either 0 or 1 expected next
	ep0Out.STAT.DTSEN = 0;

else {
	// only SETUP0 expected next
	ep0Out.STAT.DTS = 0;
	ep0Out.STAT.DTSEN = 1;
	}

ep0Out.STAT.UOWN = 1;				// must be separate instruction
}




/*	ArmEndpoint0IN
	Arm Endpoint 0 IN for the next IN transaction of a Control Read transfer
	https://stackoverflow.com/questions/3739901/when-do-usb-hosts-require-a-zero-length-in-packet-at-the-end-of-a-control-read-t
*/
static void ArmEndpoint0IN()
{
if (ep0In.STAT.UOWN) Error();

// how much data to send in the next IN transaction
ep0In.CNT =
	gEndpoint0INDataL > sizeof ep0InBuffer ?
		sizeof ep0InBuffer :
		gEndpoint0INDataL;

/* copy data into USB memory */ {
	uint8_t *to = (uint8_t*) ep0InBuffer;
	for (uint16_t i = ep0In.CNT; i > 0; i--)
		*to++ = *gEndpoint0INData++;
	gEndpoint0INDataL -= ep0In.CNT;
	}

// will send short or zero-length packet?
if (ep0In.CNT < sizeof ep0InBuffer)
	// no more data to send on this transfer
	gEndpoint0INData = NULL;

// what data to send		
ep0In.ADR = ep0InBuffer;

ep0In.STAT.i = 0;

// data toggle
ep0In.STAT.DTS = gEndpoint0INToggle;
ep0In.STAT.DTSEN = 1;
gEndpoint0INToggle = !gEndpoint0INToggle;

// 'arm' Endpoint 0 IN in anticipation of next Data Stage Transaction
ep0In.STAT.UOWN = 1;				// must be separate instruction
}


/*	ArmEndpoint0INStall
	Arm Endpoint 0 IN to stall
	Endpoint 0 OUT will be handled elsewhere
*/
static void ArmEndpoint0INStall()
{
// debugging: expect that we own this now
if (ep0In.STAT.UOWN) Error();

// set endpoint to stall the next Transaction
ep0In.STAT.i = 0;
ep0In.STAT.BSTALL = 1;
ep0In.STAT.UOWN = 1;				// must be separate instruction
}


/*	gStringDescriptor0
	*** it's a language code
*/
static const uint8_t gStringDescriptor0[4] = {
	sizeof gStringDescriptor0,
	kString,
	0x09, 0x04
	};


/*	gStringDescriptorManufacturer
 
*/
static const uint16_t gStringDescriptorManufacturer[12] = {
	(kString << 8) | sizeof gStringDescriptorManufacturer,
	'B', 'e', 'n', ' ', 'H', 'e', 'k', 's', 't', 'e', 'r'
	};


/*	gStringDescriptorProduct

*/
static const uint16_t gStringDescriptorProduct[24] = {
	(kString << 8) | sizeof gStringDescriptorProduct,
	'S', 'i', 'm', 'u', 'l', 'a', 't', 'o', 'r', ' ', 'D', 'i', 's', 'p', 'l', 'a', 'y', ' ', 'P', 'a', 'n', 'e', 'l'
	};


/*	HandleGetStringDescriptor
 
 */
static void HandleGetStringDescriptor(
	uint8_t		index
	)
{
switch (index) {
	case 0:
		gEndpoint0INData = (char*) gStringDescriptor0;
		gEndpoint0INDataL = sizeof gStringDescriptor0;
		break;
	
	case 1:
		gEndpoint0INData = (char*) gStringDescriptorManufacturer;
		gEndpoint0INDataL = sizeof gStringDescriptorManufacturer;
		break;
	
	case 2:
		gEndpoint0INData = (char*) gStringDescriptorProduct;
		gEndpoint0INDataL = sizeof gStringDescriptorProduct;
		break;
	
	default:
		Error();
		break;
	}
}


/*	HandleGetDescriptor
	
	[USB §9.4.3] 
		If the descriptor is longer than the wLength field,
		only the initial bytes of the descriptor are returned. If the descriptor is shorter than the wLength field, the
		device indicates the end of the control transfer by sending a short packet when further data is requested. A
		short packet is defined as a packet shorter than the maximum payload size or a zero length data packet.
*/
static void HandleGetDescriptor(
	const USBSetup *const setup
	)
{
// [USB Table 9.5]
switch (setup->getDescriptor.type) {
	// device descriptor?
	case kDevice:
		gEndpoint0INData = (char*) &gDeviceDescriptor;
		gEndpoint0INDataL = sizeof gDeviceDescriptor;
		break;
	
	// configuration descriptor?
	case kConfiguration:
		gEndpoint0INData = (char*) &gConfigurationDescriptor;
		gEndpoint0INDataL = sizeof gConfigurationDescriptor;
		break;
	
	// string descriptor?
	case kString:
		HandleGetStringDescriptor(setup->getDescriptor.index);
		break;
	
	// device qualifier?
	case kDeviceQualifier:
		// [USB §9.6.2] high-speed capable devices only
		// stall endpoint to signal inability to handle
		ArmEndpoint0INStall();
		break;
	
	// HID class Report Descriptor [HID §6.2.2]
	case kHIDReport:
		gEndpoint0INData = (char*) &gReportDescriptor;
		gEndpoint0INDataL = sizeof gReportDescriptor;
		break;
	
	default:
		Error();
		break;
	};

// *** I think we should not send ZLP if the amount we send back is exactly wLength

// sending data to host?
if (gEndpoint0INData)
	// don't send more than requested length
	if (gEndpoint0INDataL > setup->wLength)
		gEndpoint0INDataL = (uint8_t) setup->wLength;
}


/*	gAddressPending
	If nonzero, received the SETUP of the SetAddress
	Zero indicates no SetAddress has been received ([USB §9.4.6] states that
	"a device response to SetAddress with a value of 0 is undefined", suggesting
	that 0 is not a valid device address).
*/
static uint8_t gPendingAddress;


/*	HandleSetAddress
	[USB §9.4.6]
*/
static void HandleSetAddress(
	const USBSetup *const setup
	)
{
if (setup->wValue >= 128 || setup->wIndex || setup->wLength) {
	Error();
	return;
	}

// must not assign address until Status Stage is completed
gPendingAddress = (uint8_t) setup->wValue;
}


/*	HandleSetConfiguration
	[USB §9.4.7]
*/
static void HandleSetConfiguration(
	const USBSetup *const setup
	)
{
/* [USB §9.4.7] If wIndex, wLength, or the upper byte of wValue is non-zero, then
   the behavior of this request is not specified. */

// which configuration to apply?
switch (setup->setConfiguration.index) {
	/* [USB §9.4.7] zero places the device in its ?address state? */
	case 0:
		// disable data endpoints
		DisableEndpoint1();
		Error();
		break;
		
			
	case kConfigurationRadioPanel:
		// enable the HID data endpoint
		EnableEndpoint1();
		break;
	
	default:
		// ***** send STALL on error
		Error();
	}
}


/*	HandleEndpoint0ToDeviceStandardDevice

*/
static void HandleEndpoint0ToDeviceStandardDevice(
	const USBSetup *const setup
	)
{
switch (setup->bRequest) {
	// set address
	case kSetAddress:
		HandleSetAddress(setup);
		break;
	
	case kSetConfiguration:
		HandleSetConfiguration(setup);
		break;
	
	default:
		Error();
	}

// 'arm' Endpoint 0 IN in anticipation of Status Stage Transaction
ArmEndpoint0INStatus();
}


// feature selectors Table 9-6
enum {
	kFeatureEndpointHalt,
	kFeatureDeviceRemoteWakeup,
	kFeatureTestMode
	};


static void ClearFeatureEndpoint(
	const USBSetup *const setup
	)
{
switch (setup->wValue) {
	case kFeatureEndpointHalt:
		break;
	
	default:
		Error();
	}
}


/*	HandleEndpoint0ToDeviceStandardEndpoint

*/
static void HandleEndpoint0ToDeviceStandardEndpoint(
	const USBSetup *const setup
	)
{
switch (setup->bRequest) {
	// set address
	case kClearFeature:
		ClearFeatureEndpoint(setup);
		break;
	
	default:
		Error();
	}
}


/*	HandleHIDGetReport
 
*/
static void HandleHIDGetReport(
	const USBSetup *const setup
	)
{
Error();
}


/*	HandleHIDSetReport
	[HID §7.2.2]
*/
static void HandleHIDSetReport(
	const USBSetup *const setup
	)
{
// on report type
switch (setup->valueHigh) {
	case 2:
		// prepare to receive the output Report
		gEndpoint0OUTData = ep0OutBuffer;
		gEndpoint0OUTDataL = sizeof ep0OutBuffer;
		// ***** set the continuation or something
		break;
	
	default:
		Error();
	}
}


/*	HandleHIDSetIdle
	Limit reporting frequency [HID §7.2.4]
*/
static void HandleHIDSetIdle(
	const USBSetup *const setup
	)
{
// ***** implement
}


/*	HandleEndpoint0ToHostClassInterface
	Class-specific requests (IN, to host) [HID §7.2]
*/
static void HandleEndpoint0ToHostClassInterface(
	const USBSetup *const setup
	)
{
switch (setup->bRequest) {
	case kGetReport:
		HandleHIDGetReport(setup);
		break;
	
	default:
		Error();
	}

// 'arm' Endpoint 0 IN in anticipation of Status Stage Transaction
/* *** Note that we can only do this because we know that all the 'to-device'/Control Write transfers
   that we support right now do not send data. */
ArmEndpoint0INStatus();
}


/*	HandleEndpoint0ToDeviceClassInterface
	Class-specific requests (OUT, to device) [HID §7.2]
*/
static void HandleEndpoint0ToDeviceClassInterface(
	const USBSetup *const setup
	)
{
switch (setup->bRequest) {
	case kSetReport:
		HandleHIDSetReport(setup);
		break;
	
	case kSetIdle:
		HandleHIDSetIdle(setup);
		break;
	
	default:
		Error();
	}

// 'arm' Endpoint 0 IN in anticipation of eventual Status Stage Transaction
ArmEndpoint0INStatus();

// expect to receive data on Control Write Transfer?
if (gEndpoint0OUTData)
	// DATA1 expected first
	gEndpoint0OUTToggle = 1;
}


static void HandleEndpoint0ToHostStandardDevice(
	const USBSetup *const setup
	)
{
switch (setup->bRequest) {
	// get descriptor
	case kGetDescriptor:
		HandleGetDescriptor(setup);
		break;
	
	default:
		Error();
	}

// need to send data on Control Read Transfer?
if (gEndpoint0INData) {
	gEndpoint0INToggle = 1;		// Data 1 packet expected first
	ArmEndpoint0IN();
	}
}


static void HandleEndpoint0ToHostStandardInterface(
	const USBSetup *const setup
	)
{
switch (setup->bRequest) {
	// get descriptor
	case kGetDescriptor:
		HandleGetDescriptor(setup);
		break;
	
	default:
		Error();
	}

// need to send data on Control Read Transfer?
if (gEndpoint0INData) {
	ArmEndpoint0IN();
	}
}


/*	HandleEndpoint0SETUP
	Handle SETUP transactions on Endpoint 0
*/
static void HandleEndpoint0SETUP()
{
const USBSetup *const setup = (USBSetup*) ep0OutBuffer;

// cancel any previously in progress Control Read or Write Transfers
ep0In.STAT.UOWN = 0;
gEndpoint0INData = NULL;
gEndpoint0OUTData = NULL;

switch (setup->bmRequestType) {
	// host-to-device, Standard, Device?
	case 0b00000000:
		HandleEndpoint0ToDeviceStandardDevice(setup);
		break;
		
	case 0b00000010:
		HandleEndpoint0ToDeviceStandardEndpoint(setup);
		break;
		
	// host-to-device, Class, Interface?
	case 0b00100001:
		HandleEndpoint0ToDeviceClassInterface(setup);
		break;
	
	// device-to-host, Standard, Device?
	case 0b10000000:
		HandleEndpoint0ToHostStandardDevice(setup);
		break;
	
	// device
	case 0b10000001:
		HandleEndpoint0ToHostStandardInterface(setup);
		break;
	
	// device-to-hist, Class, Interface?
	case 0b10100001:
		HandleEndpoint0ToHostClassInterface(setup);
		break;
	
	default:
		Error();
		break;
	}

// resume processing packets again after SETUP ([PIC §24.2.1] "to allow setup processing")
UCONbits.PKTDIS = 0;
}


/*	HandleEndpoint0OUT
	Handle OUT Transactions on Endpoint 0
	
	We just completed an OUT Transaction: so, either we
	
		received data as part the Data Stage of a Control Write, or we
		completed the Status Stage of a Control Read
		
	As the handshake of a Control Read transfer [USB §8.5.3.1]
		The host may only send a zero-length data packet in this phase
		but the function may accept any length packet as a valid status inquiry.
*/
static void HandleEndpoint0OUT()
{
// Control Write Transfer? (Data Stage)
if (gEndpoint0OUTData) {
	/* Will try handling Control Writes *without* copying to a destination buffer;
	   if I like this, do the same with Control Reads (which currently *do* copy). */
	// ***** update receive pointers
	if (ep0Out.CNT == kEndpoint0MaximumPacketLength) {
		gEndpoint0OUTToggle = !gEndpoint0OUTToggle;
		}
	
	gEndpoint0INData += ep0Out.CNT;
	gEndpoint0OUTDataL -= ep0Out.CNT;
	}

// long packet; expect to receive more data on Control Write Transfer
// [***] where is it documented that we will receive ZLP in case it aligns?
else {
	// ***** handle
	
	// no more data expected
	gEndpoint0OUTData = NULL;
	
	// arm Endpoint 0 IN for Status Stage
	ArmEndpoint0INStatus();
	}
}


/*	HandleEndpoint0IN
	Handle IN Transactions on Endpoint 0
	
	We just completed an IN Transaction: so, either we
	
		sent data as part the Data Stage of a Control Read, or we
		completed the Status Stage of a Control Write
	
	*** maybe do this through pointer-to-function
*/
static void HandleEndpoint0IN()
{
// Handshake of Control Write SetAddress?
if (gPendingAddress) {
	// apply USB address to peripheral
	UADDR = gPendingAddress;
	
	// no longer any SetAddress pending
	gPendingAddress = 0;
	}

// Status Stage of Control Write Transfer?
if (gEndpoint0OUTData)
	// have already armed Endpoint 0 OUT for Setup Stage of new Control Transfer
	;

// must have been in response to a Control Read Transfer
else {
	// need to send more data?
	if (gEndpoint0INData) {
		// arm Endpoint 0 IN to send more data
		ArmEndpoint0IN();
		}
	}
}


/*	HandleUSBTransactionEndpoint0

*/
extern void HandleUSBTransactionEndpoint0()
{
// Endpoint in stalled condition?
if (UEP0bits.EPSTALL) {
	/* We intentionally set STALL in response to a GetDescriptor(DeviceQualifier)
	   SETUP Transaction.  We find EPSTALL set on the next Transaction (which is
	   a new SETUP).  Not sure if we need to build logic to detect unexpected
	   cases of EPSTALL. */
	UEP0bits.EPSTALL = 0;
	}

// *** probably should just check PID and ignore DIR
if (USTATbits.DIR == 0)
	switch (ep0Out.STAT.PID) {
		// OUT? (e.g., Handshake Transaction/Status Phase)
		case 0b0001:
			HandleEndpoint0OUT();
			break;

		// SETUP? (first Transaction of Control Transfer)
		case 0b1101:
			HandleEndpoint0SETUP();
			break;

		default:
			Error();
		}

else
	switch (ep0In.STAT.PID) {
		// IN?
		case 0b1001:
			HandleEndpoint0IN();
			break;

		default:
			Error();
		}

// arm Endpoint 0 OUT for the Status Stage of the Control Read, or an early SETUP
ArmEndpoint0OUT();
}