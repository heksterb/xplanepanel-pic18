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
		[XC8] MPLAB XC8 C Compiler User's Guide for PIC MCU
*/

#include <stdint.h>


/*	BDSTAT
	Buffer descriptor status register [PIC §24.4.1]
*/
typedef union {
	uint8_t		i;
	
	// CPU mode (writing) [PIC Register 24-5]
	struct {
		unsigned	BC8 : 1,
				BC9 : 1,
				BSTALL : 1,
				DTSEN : 1,
				: 2,
				DTS : 1,
				UOWN : 1;
		};
	
	// SIE mode (reading) [PIC Register 24-6]
	struct {
		unsigned	: 2,
				PID : 4,
				: 2;
		};
	} BDSTAT;


/*	BufferDescriptor
	Endpoint buffer descriptor [PIC §24.4]
*/
typedef struct {
	BDSTAT		STAT;
	uint8_t		CNT;
	volatile uint8_t *ADR;
	} BufferDescriptor;


/*

	Buffer Descriptor Table

*/

#if defined(__18F45K50)
#define BDT_ADDR 0x400
#endif

volatile BufferDescriptor
	ep0Out __at(BDT_ADDR + 0),		// buffer descriptor Endpoint 0 OUT
	ep0In __at(BDT_ADDR + 4),		// buffer descriptor Endpoint 0 IN
	ep1Out __at(BDT_ADDR + 8),
	ep1In __at(BDT_ADDR + 12);

// buffer sizes have to agree with gDeviceDescriptor.maxPacketSize0
// must be one of 8, 16, 32, or 64 [USB Table 9-8]
volatile uint8_t
	ep0OutBuffer[32] __at(BDT_ADDR + 16),
	ep0InBuffer[32] __at(BDT_ADDR + 48),
	ep1OutBuffer[5] __at(BDT_ADDR + 80),
	ep1InBuffer[5] __at(BDT_ADDR + 85);


/*	USBSetup
	Setup transaction data [USB §9.3]
*/
typedef struct {
	// bmRequestType
	union {
		unsigned	bmRequestType : 8;
		
		struct {
			unsigned	direction : 1,
					type : 2,
					recipient : 5;
			};
		};
	
	unsigned	bRequest : 8;
	
	// wValue
	union {
		// generic
		uint16_t	wValue;
		
		struct {
			unsigned	valueLow : 8,
					valueHigh : 8;
			};
		
		struct {
			unsigned	index : 8,		// low byte
					: 8;			// high byte
			} setConfiguration;
		
		struct {
			unsigned	index : 8,		// low byte
					type : 8;		// high byte
			} getDescriptor;
		};
	
	uint16_t	wIndex;
	uint16_t	wLength;
	} USBSetup;



/*	SetupRequest
	Standard requests
	[USB Table 9-4]
*/
typedef enum {
	kGetStatus,
	kClearFeature,
	kReserved0,
	kSetFeature,
	kReserved1,
	kSetAddress,
	kGetDescriptor,
	kSetDescriptor,
	kGetConfiguration,
	kSetConfiguration,
	kGetInterface,
	kSetInterface,
	kSyncFrame
	} SetupRequest;


/*	DescriptorType

	[USB Table 9-5]
	XC8 makes this unsigned char
*/
typedef enum {
	kNone,
	kDevice,
	kConfiguration,
	kString,
	kInterface,
	kEndpoint,
	kDeviceQualifier,
	kOtherSpeedConfiguration,
	kInterfacePower,
	
	/* [HID §7.1] */
	kHID = 0x21,
	kHIDReport,
	kHIDPhysical
	} DescriptorType;


/*	DeviceDescriptor
	USB device descriptor
	[USB Table 9.8]
*/
typedef struct {
	uint8_t		length;
	DescriptorType	type;
	uint16_t	usb;
	uint8_t		klass,
			subClass;
	uint8_t		protocol;
	uint8_t		maxPacketSize0;
	uint16_t	vendorID,
			productID;
	uint16_t	device;
	uint8_t		manufacturerI,
			productI,
			serialNumberI;
	uint8_t		configurationsN;
	} DeviceDescriptor;


/*	ConfigurationDescriptor
	[USB §9.6.3]
*/
typedef struct {
	uint8_t		length;
	DescriptorType	type;
	uint16_t	totalLength;
	uint8_t		interfacesN;
	uint8_t		configurationValue;
	uint8_t		configurationI;
	unsigned	reserved0 : 5,
			remoteWakeup : 1,
			selfPowered : 1,
			reserved1 : 1;
	uint8_t		maxPower;
	} ConfigurationDescriptor;


/*	InterfaceClass
	Interface descriptor class [USB §]
*/
typedef enum /* unsigned char */ {
	kInterfaceClassReserved,
	kInterfaceClassAudio,
	kInterfaceClassCommunicationsCommunications,
	kInterfaceClassHID,
	kInterfaceClassPhysicial,
	kInterfaceClassImage,
	kInterfaceClassPrinter,
	kInterfaceClassMassStorage,
	kInterfaceClassHub,
	kInterfaceClassCommunicationsData,
	kInterfaceClassSmartCard,
	kInterfaceClassContentSecurity,
	kInterfaceClassVideo,
	kInterfaceClassPersonalHealthcare,
	kInterfaceClassDiagnostic = 0xDC,
	kInterfaceClassWireless = 0xE0,
	kInterfaceClassMiscellaneous = 0xEF,
	kInterfaceClassAppplication = 0xFE,
	kInterfaceClassVendor = 0xFF
	} InterfaceClass;


/*	InterfaceDescriptor
	[USB §9.6.5]
*/
typedef struct {
	uint8_t		length;
	DescriptorType	type;
	uint8_t		number;
	uint8_t		alternateSetting;
	uint8_t		endpointsN;
	InterfaceClass	klass;
	uint8_t		subclass;
	uint8_t		protocol;
	uint8_t		stringI;
	} InterfaceDescriptor;



/*	HIDClassDescriptor1
	[USBHID §6.2.1]
*/
typedef struct {
	uint8_t		length;
	DescriptorType	type;
	uint16_t	hidVersion;
	uint8_t		countryCode;
	uint8_t		descriptorsN;
	
	struct {
		DescriptorType	type;
		uint16_t	length;
		} item[1];
	} HIDClassDescriptor1;


/*	EndpointDescriptor

*/
typedef struct {
	// direction
	enum { kOUT, kIN };
	
	// transfer
	enum { kControl, kIsochronous, kBulk, kInterrupt };
	
	// synchronization
	enum { kNoSynchronization, kAsynchronous, kAdaptive, kSynchronous };
	
	// usage
	enum { kData, kFeedback, kImplicitFeedback, kReserved };
	
	uint8_t		length;
	DescriptorType	type;
	unsigned	number : 4,
			reserved0 : 3,
			direction : 1;
	unsigned	transfer : 2,
			synchronization : 2,
			usage : 2,
			reserved1 : 2;
	
	uint16_t	maxPacketSize;
	uint8_t		interval;
	} EndpointDescriptor;


/*	HIDReportDescriptorItem
	Base class of all USB HID Report Descriptor Items
*/
typedef struct {
	// type [HID §6.2.2.2]
	enum { kMain, kGlobal, kLocal };

	// [HID §6.2.2.4] Main Item Tags
	enum {
		kInput = 8,
		kOutput,
		kCollection,			// note order reversed from [HID] listing
		kFeature,
		kCollectionEnd
		};
	
	// [HID §6.2.2.6] values for Collection Items
	enum {
		kCollectionPhysical,
		kCollectionApplication,
		kCollectionLogical,
		kCollectionReport,
		kCollectionNamedArray,
		kCollectionUsageSwitch,
		kCollectionUsageModifier
		};
	
	// [HID §6.2.2.7] Global Item Tags
	enum {
		kUsageGlobal,
		kLogicalMinimum,
		kLogicalMaximum,
		kPhysicalMinimum,
		kPhysicalMaximum,
		kUnitExponent,
		kUnit,
		kReportSize,
		kReportID,
		kReportCount,
		kPush,
		kPop
		};
	
	// [HID §6.2.2.8] Local Item Tags
	enum {
		kUsageLocal,
		kUsageMinimum,
		kUsageMaximum,
		kDesignatorIndex,
		kDesignatorMinimum,
		kDesignatorMaximum,
		kStringIndex,
		kStringMinimum,
		kStringMaximum,
		kDelimiter
		};
	
	unsigned	size2 : 2,
			type : 2,
			tag : 4;
	} HIDReportDescriptorItemPrefix;


typedef struct {
	HIDReportDescriptorItemPrefix prefix;
	} HIDReportDescriptorItem0;


typedef struct {
	HIDReportDescriptorItemPrefix prefix;
	uint8_t		value;
	} HIDReportDescriptorItem8;


typedef struct {
	HIDReportDescriptorItemPrefix prefix;
	uint16_t	value;
	} HIDReportDescriptorItem16;


typedef struct {
	HIDReportDescriptorItemPrefix prefix;
	uint32_t	value;
	} HIDReportDescriptorItem32;


/*	ClassSetupRequest
	[HID §7.2]
*/
typedef enum {
	kGetReport = 1,
	kGetIdle,
	kGetProtocol,
	kSetReport = 9,
	kSetIdle,
	kSetProtocol
	} ClassSetupRequest;


extern void USBInitialize(void);
extern void USBInterruptService(void);
extern void Error(void);
