/*
 * VUEngine VirtualBoy
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// HARDWARE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// Hardware register mnemonics
#define	__CCR				0x00	// Communication Control Register	(0x0200 0000)
#define	__CCSR				0x04	// COMCNT Control Register			(0x0200 0004)
#define	__CDTR				0x08	// Transmitted Data Register		(0x0200 0008)
#define	__CDRR				0x0C	// Received Data Register			(0x0200 000C)
#define	__SDLR				0x10	// Serial Data Low Register			(0x0200 0010)
#define	__SDHR				0x14	// Serial Data High Register		(0x0200 0014)
#define	__TLR				0x18	// Timer Low Register				(0x0200 0018)
#define	__THR				0x1C	// Timer High Register				(0x0200 001C)
#define	__TCR				0x20	// Timer Control Register			(0x0200 0020)
#define	__WCR				0x24	// Wait-state Control Register		(0x0200 0024)
#define	__SCR				0x28	// Serial Control Register			(0x0200 0028)

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CPU
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __CPU_GET_STACK_POINTER(sp)			asm("mov	sp, %0": "=r" (sp))
#define __CPU_GET_LINK_POINTER(sp)			asm("mov	lp, %0": "=r" (lp))

#define __CPU_HALT							asm("halt"::)
#define __CPU_ENABLE_INTERRUPTS				asm("cli")
#define __CPU_SUSPEND_INTERRUPTS			asm("sei")

#define __CPU_SET_INTERRUPT_LEVEL(level)	asm									\
											(									\
												"stsr	sr5, r6			\n\t"	\
												"movhi	0xFFF1, r0, r7	\n\t"	\
												"movea	0xFFFF, r7, r7	\n\t"	\
												"and	r6, r7			\n\t"	\
												"mov	%0,r6			\n\t"	\
												"andi	0x000F, r6, r6	\n\t"	\
												"shl	0x10, r6		\n\t"	\
												"or		r7, r6			\n\t"	\
												"ldsr	r6, sr5			\n\t"	\
												: 								\
												: "r" (level)					\
												: "r6", "r7"					\
											)

#define __CPU_MULTIPLEX_INTERRUPTS			uint32 psw;							\
											asm									\
											(									\
												"stsr	psw, %0"				\
												: "=r" (psw) 					\
											);									\
											psw &= 0xFFF0BFFF;					\
											asm									\
											(									\
												"ldsr	%0, psw	\n\t"			\
												"cli	\n\t"					\
												: 								\
												: "r" (psw)						\
												: 								\
											)

// Cache management
#define CACHE_ENABLE		asm("mov 2,r1 \n  ldsr r1,sr24": /* No Output */: /* No Input */: "r1" /* Reg r1 Used */)
#define CACHE_DISABLE		asm("ldsr r0,sr24")
#define CACHE_CLEAR			asm("mov 1,r1 \n  ldsr r1,sr24": /* No Output */: /* No Input */: "r1" /* Reg r1 Used */)
#define CACHE_RESET			CACHE_DISABLE; CACHE_CLEAR; CACHE_ENABLE

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// KEYPAD
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// hardware reg __SCR specs
#define __S_INTDIS		0x80	 // Disable Interrupts
#define __S_SW			0x20	 // Software Reading
#define __S_SWCK		0x10	 // Software Clock, Interrupt?
#define __S_HW			0x04	 // Hardware Reading
#define __S_STAT		0x02	 // Hardware Reading Status
#define __S_HWDIS		0x01	 // Disable Hardware Reading

// keypad specs
#define K_NON			0x0000	// No key
#define K_PWR			0x0001  // Low Power
#define K_SGN			0x0002  // Signature; 1 = Standard Pad
#define K_A				0x0004  // A Button
#define K_B				0x0008  // B Button
#define K_RT			0x0010  // R Trigger
#define K_LT			0x0020  // L Trigger
#define K_RU			0x0040  // Right Pad, Up
#define K_RR			0x0080  // Right Pad, Right
#define K_LR			0x0100  // Left Pad, Right
#define K_LL			0x0200  // Left Pad, Left
#define K_LD			0x0400  // Left Pad, Down
#define K_LU			0x0800  // Left Pad, Up
#define K_STA			0x1000  // Start Button
#define K_SEL			0x2000  // Select Button
#define K_RL			0x4000  // Right Pad, Left
#define K_RD			0x8000  // Right Pad, Down
#define K_ANY			0xFFFC  // All keys, without pwr & sgn
#define K_BTNS			0x303C  // All buttons; no d-pads, pwr or sgn
#define K_PADS			0xCFC0  // All d-pads
#define K_LPAD			0x0F00  // Left d-pad only
#define K_RPAD			0xC0C0  // Right d-pad only

#define __KEY_NONE		0x0000
#define __KEY_PRESSED   0x0001
#define __KEY_RELEASED  0x0010
#define __KEY_HOLD		0x0100

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// FRAME BUFFER
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __FRAME_BUFFERS_SIZE			0x6000

#define __LEFT_FRAME_BUFFER_0			0x00000000	// Left Frame Buffer 0
#define __LEFT_FRAME_BUFFER_1			0x00008000	// Left Frame Buffer 1
#define __RIGHT_FRAME_BUFFER_0			0x00010000	// Right Frame Buffer 0
#define __RIGHT_FRAME_BUFFER_1			0x00018000	// Right Frame Buffer 1

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// TIMER
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __TIMER_COUNTER_DELTA						1

// Use with 20us timer (range = 0 to 1300)
#define __TIME_US(n)			(((n) / Timer_getResolutionInUS()) 														\
													- __TIMER_COUNTER_DELTA)
#define __TIME_INVERSE_US(n)	((n + __TIMER_COUNTER_DELTA) * 															\
													Timer_getResolutionInUS())

// Use with 100us timer (range = 0 to 6500, and 0 to 6.5)
#define __TIME_MS(n)			((((n) * __MICROSECONDS_PER_MILLISECOND) / 												\
													Timer_getResolutionInUS()) - __TIMER_COUNTER_DELTA)

#define __TIME_INVERSE_MS(n)	((n + __TIMER_COUNTER_DELTA) * 															\
													Timer_getResolutionInUS() / 1000)

#define __TIMER_ENB									0x01
#define __TIMER_Z_STAT								0x02
#define __TIMER_Z_STAT_ZCLR							0x04
#define __TIMER_Z_INT								0x08
#define __TIMER_20US								0x10
#define __TIMER_100US								0x00

enum TimerResolutionScales
{
	kUS = 0,			// Microseconds
	kMS,				// Milliseconds
};

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// RUMBLE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __RUMBLE_MAX_EFFECTS_IN_CHAIN		  8
#define __RUMBLE_MAX_OVERDRIVE				  126
#define __RUMBLE_CHAIN_EFFECT_0				  0x00
#define __RUMBLE_CHAIN_EFFECT_1				  0x01
#define __RUMBLE_CHAIN_EFFECT_2				  0x02
#define __RUMBLE_CHAIN_EFFECT_3				  0x03
#define __RUMBLE_CHAIN_EFFECT_4				  0x04
#define __RUMBLE_FREQ_50HZ					  0x04
#define __RUMBLE_FREQ_95HZ					  0x05
#define __RUMBLE_FREQ_130HZ					  0x06
#define __RUMBLE_FREQ_160HZ					  0x00
#define __RUMBLE_FREQ_240HZ					  0x01
#define __RUMBLE_FREQ_320HZ					  0x02
#define __RUMBLE_FREQ_400HZ					  0x03
#define __RUMBLE_CMD_STOP					  0x00
#define __RUMBLE_CMD_MIN_EFFECT				  0x01
#define __RUMBLE_CMD_MAX_EFFECT				  0x7B
#define __RUMBLE_CMD_PLAY					  0x7C
#define __RUMBLE_CMD_CHAIN_EFFECT_0			  0x80
#define __RUMBLE_CMD_CHAIN_EFFECT_1			  0x81
#define __RUMBLE_CMD_CHAIN_EFFECT_2			  0x82
#define __RUMBLE_CMD_CHAIN_EFFECT_3			  0x83
#define __RUMBLE_CMD_CHAIN_EFFECT_4			  0x84
#define __RUMBLE_CMD_FREQ_50HZ				  0x87
#define __RUMBLE_CMD_FREQ_95HZ				  0x88
#define __RUMBLE_CMD_FREQ_130HZ				  0x89
#define __RUMBLE_CMD_FREQ_160HZ				  0x90
#define __RUMBLE_CMD_FREQ_240HZ				  0x91
#define __RUMBLE_CMD_FREQ_320HZ				  0x92
#define __RUMBLE_CMD_FREQ_400HZ				  0x93
#define __RUMBLE_CMD_OVERDRIVE				  0xA0
#define __RUMBLE_CMD_SUSTAIN_POS			  0xA1
#define __RUMBLE_CMD_SUSTAIN_NEG			  0xA2
#define __RUMBLE_CMD_BREAK					  0xA3
#define __RUMBLE_CMD_WRITE_EFFECT_CHAIN		  0xB0
#define __RUMBLE_CMD_WRITE_EFFECT_LOOPS_CHAIN 0xB1
#define __RUMBLE_EFFECT_CHAIN_END			  0xFF
#define __RUMBLE_TOTAL_COMMANDS				  10

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// SOUND UNIT
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __DEFAULT_PCM_HZ						8000
#define __TOTAL_WAVEFORMS						5
#define __TOTAL_SOUND_SOURCES					6
#define __TOTAL_MODULATION_CHANNELS 			1
#define __TOTAL_NOISE_CHANNELS					1
#define __TOTAL_NORMAL_CHANNELS					(__TOTAL_SOUND_SOURCES - __TOTAL_MODULATION_CHANNELS - __TOTAL_NOISE_CHANNELS)
#define __TOTAL_POTENTIAL_NORMAL_CHANNELS 		(__TOTAL_NORMAL_CHANNELS + __TOTAL_MODULATION_CHANNELS)
#define __MAXIMUM_VOLUME						0xF

/// Sound source types
/// @memberof VSUManager
enum SoundSourceTypes
{
	kSoundSourceNormal = (1 << 0),
	kSoundSourceModulation = (1 << 1),
	kSoundSourceNoise = (1 << 2),
};

/// Playback types
/// @memberof VSUManager
enum VSUPlaybackModes
{
	kPlaybackNative = 0,
	kPlaybackPCM
};

/// A struct that holds the Waveform Data
/// @memberof VSUManager
typedef struct WaveformData
{
	/// Waveform's data
	int8 data[32];

	/// Data's CRC
	uint32 crc;

} WaveformData;

/// A Waveform struct
/// @memberof VSUManager
typedef struct Waveform
{
	/// Waveform's index
	uint8 index;

	/// Count of channels using this waveform
	int8 usageCount;

	/// Pointer to the VSU's waveform address
	uint8* wave;

	/// Pointer to the waveform data
	const int8* data;

	/// Data's CRC
	uint32 crc;

} Waveform;

/// Sound source mapping
/// @memberof VSUManager
typedef struct SoundSource
{
	uint8 SxINT;
	uint8 spacer1[3];
	uint8 SxLRV;
	uint8 spacer2[3];
	uint8 SxFQL;
	uint8 spacer3[3];
	uint8 SxFQH;
	uint8 spacer4[3];
	uint8 SxEV0;
	uint8 spacer5[3];
	uint8 SxEV1;
	uint8 spacer6[3];
	uint8 SxRAM;
	uint8 spacer7[3];
	uint8 SxSWP;
	uint8 spacer8[35];
} SoundSource;

/// Sound source configuration
/// @memberof VSUManager
typedef struct SoundSourceConfiguration
{
	/// Requester Id
	uint32 requesterId;

	/// VSU sound source to configure
	SoundSource* soundSource;

	/// VSU waveform to use
	const Waveform* waveform;

	/// Time when the configuration elapses
	fix7_9_ext timeout;

	/// Sound source type
	uint32 type;

	/// SxINT values
	uint8 SxINT;

	/// SxLRV values
	uint8 SxLRV;

	/// SxFQL values
	uint8 SxFQL;

	/// SxFQH values
	uint8 SxFQH;

	/// SxEV0 values
	uint8 SxEV0;

	/// SxEV1 values
	uint8 SxEV1;

	/// SxRAM index
	uint8 SxRAM;

	/// SxSWP values
	uint8 SxSWP;

	/// SxMOD pointer
	const int8* SxMOD;

	/// Priority for sound channel usage
	uint8 priority;

	/// Skip if no sound source available?
	bool skip;

} SoundSourceConfiguration;

/// Sound source configuration request
/// @memberof VSUManager
typedef struct SoundSourceConfigurationRequest
{
	/// Requester Id
	uint32 requesterId;

	/// Time when the configuration elapses
	fix7_9_ext timeout;

	/// Sound source type
	uint32 type;

	/// SxINT values
	uint8 SxINT;

	/// SxLRV values
	uint8 SxLRV;

	/// SxFQL values
	uint8 SxFQL;

	/// SxFQH values
	uint8 SxFQH;

	/// SxEV0 values
	uint8 SxEV0;

	/// SxEV1 values
	uint8 SxEV1;

	/// SxRAM index
	const WaveformData* SxRAM;

	/// SxSWP values
	uint8 SxSWP;

	/// SxMOD pointer
	const int8* SxMOD;

	/// Priority for sound channel usage
	uint8 priority;

	/// Skip if no sound source available?
	bool skip;

} SoundSourceConfigurationRequest;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// DISPLAY UNIT
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// Start address of WORLD space
#define __WORLD_SPACE_BASE_ADDRESS				0x0003D800	
// Start address of OBJECT space
#define __OBJECT_SPACE_BASE_ADDRESS 			0x0003E000	

#define __WORLD_OFF								0x0000
#define __WORLD_ON								0xC000
#define __WORLD_LON								0x8000
#define __WORLD_RON								0x4000
#define __WORLD_OBJECT							0x3000
#define __WORLD_AFFINE							0x2000
#define __WORLD_HBIAS							0x1000
#define __WORLD_BGMAP							0x0000

#define __WORLD_1x1								0x0000
#define __WORLD_1x2								0x0100
#define __WORLD_1x4								0x0200
#define __WORLD_1x8								0x0300
#define __WORLD_2x1								0x0400
#define __WORLD_2x2								0x0500
#define __WORLD_2x4								0x0600
#define __WORLD_4x1								0x0800
#define __WORLD_4x2								0x0900
#define __WORLD_8x1								0x0C00

#define __WORLD_OVR								0x0080
#define __WORLD_END								0x0040

#define __BRTA						0x12  // Brightness A
#define __BRTB						0x13  // Brightness B
#define __BRTC						0x14  // Brightness C

#define __GPLT0						0x30  // BGMap Palette 0
#define __GPLT1						0x31  // BGMap Palette 1
#define __GPLT2						0x32  // BGMap Palette 2
#define __GPLT3						0x33  // BGMap Palette 3

#define __JPLT0						0x34  // OBJ Palette 0
#define __JPLT1						0x35  // OBJ Palette 1
#define __JPLT2						0x36  // OBJ Palette 2
#define __JPLT3						0x37  // OBJ Palette 3

// Column table
#define __COLUMN_TABLE_ENTRIES				256
#define __BRIGHTNESS_REPEAT_ENTRIES 		96

/// Represents an entry in WORLD space in DRAM
/// @memberof DisplayUnit
typedef struct WorldAttributes
{
	uint16 head;
	int16 gx;
	int16 gp;
	int16 gy;
	uint16 mx;
	int16 mp;
	uint16 my;
	uint16 w;
	uint16 h;
	uint16 param;
	uint16 ovr;
	uint16 spacer[5];

} WorldAttributes;

/// Represents an entry in OBJECT space in DRAM
/// @memberof DisplayUnit
typedef struct ObjectAttributes
{
	int16 jx;
	int16 head;
	int16 jy;
	int16 tile;

} ObjectAttributes;

// Pointers to access DRAM caches
extern WorldAttributes _worldAttributesCache[__TOTAL_LAYERS];
extern ObjectAttributes _objectAttributesCache[__TOTAL_OBJECTS];

/// Column table specification
/// @memberof DisplayUnit
typedef struct ColumnTableSpec
{
	/// Defines whether the spec's first half should be mirrored (true)
	/// or if a full 256 entry table is provided (false)
	bool mirror;

	/// Column table entries array
	uint8 columnTable[__COLUMN_TABLE_ENTRIES];

} ColumnTableSpec;

/// A ColumnTable spec that is stored in ROM
/// @memberof DisplayUnit
typedef const ColumnTableSpec ColumnTableROMSpec;

/// Brigtness control specification
/// @memberof DisplayUnit
typedef struct BrightnessRepeatSpec
{
	/// Defines whether the spec's first half should be mirrored (true)
	/// or if a full 96 entry table is provided (false)
	bool mirror;

	/// Brightness repeat values
	uint8 brightnessRepeat[__BRIGHTNESS_REPEAT_ENTRIES];

} BrightnessRepeatSpec;

/// A BrightnessRepeat spec that is stored in ROM
/// @memberof DisplayUnit
typedef const BrightnessRepeatSpec BrightnessRepeatROMSpec;

/// Color configuration struct
/// @memberof DisplayUnit
typedef struct ColorConfig
{
	/// Background color
	uint8 backgroundColor;

	// Brightness config
	Brightness brightness;

	// Brightness repeat values
	BrightnessRepeatSpec* brightnessRepeat;

} ColorConfig;

/// Palette configuration struct
/// @memberof DisplayUnit
typedef struct PaletteConfig
{
	struct Bgmap
	{
		uint8 gplt0;
		uint8 gplt1;
		uint8 gplt2;
		uint8 gplt3;
	} bgmap;

	struct Object
	{
		uint8 jplt0;
		uint8 jplt1;
		uint8 jplt2;
		uint8 jplt3;
	} object;

} PaletteConfig;

/// Enums used to control VIP interrupts
/// @memberof DisplayUnit
enum VIPMultiplexedInterrupts
{
	kVIPNoMultiplexedInterrupts 			= 1 << 0,
	kVIPOnlyVIPMultiplexedInterrupts 		= 1 << 2,
	kVIPOnlyNonVIPMultiplexedInterrupts 	= 1 << 3,
	kVIPAllMultiplexedInterrupts 			= 0x7FFFFFFF,
};

#endif
