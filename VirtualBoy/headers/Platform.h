/*
 * VUEngine VirtualBoy
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CPU
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __CAPTURE_EXCEPTION_REGISTERS											\
	asm(" mov sp,%0  ": "=r" (_exceptionStackPointer));							\
	asm(" mov lp,%0  ": "=r" (_exceptionLinkPointer));							\

#define __CPU_GET_STACK_POINTER(sp)			asm("mov	sp, %0": "=r" (sp))
#define __CPU_GET_LINK_POINTER(lp)			asm("mov	lp, %0": "=r" (lp))

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
#define CACHE_ENABLE						asm("mov 2,r1 \n  ldsr r1,sr24": /* No Output */: /* No Input */: "r1" /* Reg r1 Used */)
#define CACHE_DISABLE						asm("ldsr r0,sr24")
#define CACHE_CLEAR							asm("mov 1,r1 \n  ldsr r1,sr24": /* No Output */: /* No Input */: "r1" /* Reg r1 Used */)
#define CACHE_RESET							CACHE_DISABLE; CACHE_CLEAR; CACHE_ENABLE

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// KEYPAD
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// keypad specs
#define K_NON								0x0000	// No key
#define K_PWR								0x0001  // Low Power
#define K_SGN								0x0002  // Signature; 1 = Standard Pad
#define K_A									0x0004  // A Button
#define K_B									0x0008  // B Button
#define K_RT								0x0010  // R Trigger
#define K_LT								0x0020  // L Trigger
#define K_RU								0x0040  // Right Pad, Up
#define K_RR								0x0080  // Right Pad, Right
#define K_LR								0x0100  // Left Pad, Right
#define K_LL								0x0200  // Left Pad, Left
#define K_LD								0x0400  // Left Pad, Down
#define K_LU								0x0800  // Left Pad, Up
#define K_STA								0x1000  // Start Button
#define K_SEL								0x2000  // Select Button
#define K_RL								0x4000  // Right Pad, Left
#define K_RD								0x8000  // Right Pad, Down
#define K_ANY								0xFFFC  // All keys, without pwr & sgn
#define K_BTNS								0x303C  // All buttons; no d-pads, pwr or sgn
#define K_PADS								0xCFC0  // All d-pads
#define K_LPAD								0x0F00  // Left d-pad only
#define K_RPAD								0xC0C0  // Right d-pad only

#define __KEY_NONE							0x0000
#define __KEY_PRESSED   					0x0001
#define __KEY_RELEASED  					0x0010
#define __KEY_HOLD							0x0100

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// TIMER
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __TIMER_20US						0x10
#define __TIMER_100US						0x00

enum TimerResolutionScales
{
	kUS = 0,			// Microseconds
	kMS,				// Milliseconds
};

typedef struct TimerConfig
{
	/// Timer's resolution (__TIMER_100US or __TIMER_20US)
	uint16 resolution;

	/// Target elapsed time between timer interrupts
	uint16 targetTimePerInterrupt;

	/// Timer interrupt's target time units
	uint16 targetTimePerInterrupttUnits;

} TimerConfig;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// SOUND UNIT
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __TOTAL_SOUND_SOURCES					6
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

#define __BRIGHTNESS_REPEAT_ENTRIES 			96
#define __COLUMN_TABLE_ENTRIES					256
#define __TOTAL_OBJECT_SEGMENTS 				4
#define __TOTAL_WORLD_LAYERS 					32

// Start address for TILE memory
#define __TILE_SPACE_BASE_ADDRESS				0x00078000
#define __TEXTURE_SPACE_BASE_ADDRESS			0x00020000

/// Brightness settings
/// @memberof DisplayUnit
typedef struct Brightness
{
	uint8 darkRed;
	uint8 mediumRed;
	uint8 brightRed;

} Brightness;

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

/// Configuration specification for the containers that manage STPs
/// @memberof DisplayUnit
typedef struct ObjectSpritesConfig
{
	/// Instantiate
	bool instantiate;

	/// Z position
	int16 zPosition;

} ObjectSpritesConfig;

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

	/// Color configuration data
/// @memberof DisplayUnit
typedef struct DisplayColorConfig
{
	/// Pointer to the column table's configuration data
	const ColumnTableSpec* columnTableSpec;

	/// Color configuration
	ColorConfig colorConfig;

	/// Palettes' configuration
	PaletteConfig paletteConfig;

} DisplayColorConfig;

/// DisplayUnit's configuration specification
/// @memberof DisplayUnit
typedef struct DisplayUnitConfig
{
	/// Color configuration data
	DisplayColorConfig displayColorConfig;
	
	/// Number of BGMAP segments reserved for the param tables
	int32 paramTableSegments;

	/// Object Sprite Containers configuration
	ObjectSpritesConfig objectSpritesContainersConfiguration[__TOTAL_OBJECT_SEGMENTS];

} DisplayUnitConfig;

#endif
