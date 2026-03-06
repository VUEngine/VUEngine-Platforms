/*
 * VUEngine VirtualBoy
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 */

#ifndef VIRTUAL_BOY_CONFIG_H_
#define VIRTUAL_BOY_CONFIG_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <Platform.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// HARDWARE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// Hardware register mnemonics
#define	__CCR									0x00	// Communication Control Register	(0x0200 0000)
#define	__CCSR									0x04	// COMCNT Control Register			(0x0200 0004)
#define	__CDTR									0x08	// Transmitted Data Register		(0x0200 0008)
#define	__CDRR									0x0C	// Received Data Register			(0x0200 000C)
#define	__SDLR									0x10	// Serial Data Low Register			(0x0200 0010)
#define	__SDHR									0x14	// Serial Data High Register		(0x0200 0014)
#define	__TLR									0x18	// Timer Low Register				(0x0200 0018)
#define	__THR									0x1C	// Timer High Register				(0x0200 001C)
#define	__TCR									0x20	// Timer Control Register			(0x0200 0020)
#define	__WCR									0x24	// Wait-state Control Register		(0x0200 0024)
#define	__SCR									0x28	// Serial Control Register			(0x0200 0028)

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// TIMER
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __TIMER_COUNTER_DELTA					1

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

#define __TIMER_ENB								0x01
#define __TIMER_Z_STAT							0x02
#define __TIMER_Z_STAT_ZCLR						0x04
#define __TIMER_Z_INT							0x08

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// KEYPAD
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// hardware reg __SCR specs
#define __S_INTDIS								0x80	 // Disable Interrupts
#define __S_SW									0x20	 // Software Reading
#define __S_SWCK								0x10	 // Software Clock, Interrupt?
#define __S_HW									0x04	 // Hardware Reading
#define __S_STAT								0x02	 // Hardware Reading Status
#define __S_HWDIS								0x01	 // Disable Hardware Reading

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// RUMBLE
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __RUMBLE_MAX_EFFECTS_IN_CHAIN			8
#define __RUMBLE_MAX_OVERDRIVE					126
#define __RUMBLE_CHAIN_EFFECT_0					0x00
#define __RUMBLE_CHAIN_EFFECT_1					0x01
#define __RUMBLE_CHAIN_EFFECT_2					0x02
#define __RUMBLE_CHAIN_EFFECT_3					0x03
#define __RUMBLE_CHAIN_EFFECT_4					0x04
#define __RUMBLE_FREQ_50HZ						0x04
#define __RUMBLE_FREQ_95HZ						0x05
#define __RUMBLE_FREQ_130HZ						0x06
#define __RUMBLE_FREQ_160HZ						0x00
#define __RUMBLE_FREQ_240HZ						0x01
#define __RUMBLE_FREQ_320HZ						0x02
#define __RUMBLE_FREQ_400HZ						0x03
#define __RUMBLE_CMD_STOP						0x00
#define __RUMBLE_CMD_MIN_EFFECT					0x01
#define __RUMBLE_CMD_MAX_EFFECT					0x7B
#define __RUMBLE_CMD_PLAY						0x7C
#define __RUMBLE_CMD_CHAIN_EFFECT_0				0x80
#define __RUMBLE_CMD_CHAIN_EFFECT_1				0x81
#define __RUMBLE_CMD_CHAIN_EFFECT_2				0x82
#define __RUMBLE_CMD_CHAIN_EFFECT_3				0x83
#define __RUMBLE_CMD_CHAIN_EFFECT_4				0x84
#define __RUMBLE_CMD_FREQ_50HZ					0x87
#define __RUMBLE_CMD_FREQ_95HZ					0x88
#define __RUMBLE_CMD_FREQ_130HZ					0x89
#define __RUMBLE_CMD_FREQ_160HZ					0x90
#define __RUMBLE_CMD_FREQ_240HZ					0x91
#define __RUMBLE_CMD_FREQ_320HZ					0x92
#define __RUMBLE_CMD_FREQ_400HZ					0x93
#define __RUMBLE_CMD_OVERDRIVE					0xA0
#define __RUMBLE_CMD_SUSTAIN_POS				0xA1
#define __RUMBLE_CMD_SUSTAIN_NEG				0xA2
#define __RUMBLE_CMD_BREAK						0xA3
#define __RUMBLE_CMD_WRITE_EFFECT_CHAIN			0xB0
#define __RUMBLE_CMD_WRITE_EFFECT_LOOPS_CHAIN 	0xB1
#define __RUMBLE_EFFECT_CHAIN_END				0xFF
#define __RUMBLE_TOTAL_COMMANDS					10

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

#define __BRTA									0x12  // Brightness A
#define __BRTB									0x13  // Brightness B
#define __BRTC									0x14  // Brightness C

#define __GPLT0									0x30  // BGMap Palette 0
#define __GPLT1									0x31  // BGMap Palette 1
#define __GPLT2									0x32  // BGMap Palette 2
#define __GPLT3									0x33  // BGMap Palette 3

#define __JPLT0									0x34  // OBJ Palette 0
#define __JPLT1									0x35  // OBJ Palette 1
#define __JPLT2									0x36  // OBJ Palette 2
#define __JPLT3									0x37  // OBJ Palette 3

#define __SPT0									0x24  // OBJ Group 0 Pointer
#define __SPT1									0x25  // OBJ Group 1 Pointer
#define __SPT2									0x26  // OBJ Group 2 Pointer
#define __SPT3									0x27  // OBJ Group 3 Pointer

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
extern WorldAttributes _worldAttributesCache[__TOTAL_WORLD_LAYERS];
extern ObjectAttributes _objectAttributesCache[__TOTAL_OBJECTS];

/// Enums used to control VIP interrupts
/// @memberof DisplayUnit
enum VIPMultiplexedInterrupts
{
	kVIPNoMultiplexedInterrupts 			= 1 << 0,
	kVIPOnlyVIPMultiplexedInterrupts 		= 1 << 2,
	kVIPOnlyNonVIPMultiplexedInterrupts 	= 1 << 3,
	kVIPAllMultiplexedInterrupts 			= 0x7FFFFFFF,
};

/// Represent the m coordinates of the textures in BGMAP space
/// @memberof DisplayUnit
typedef struct BgmapTextureSource
{
	int16 mx;
	int16 mp;
	int16 my;

} BgmapTextureSource;

/// Represent the displacement of graphics data in map arrays
/// @memberof DisplayUnit
typedef struct ObjectTextureSource
{
	int32 displacement;

} ObjectTextureSource;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// FRAME BUFFER
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __FRAME_BUFFERS_SIZE					0x6000

#define __LEFT_FRAME_BUFFER_0					0x00000000	// Left Frame Buffer 0
#define __LEFT_FRAME_BUFFER_1					0x00008000	// Left Frame Buffer 1
#define __RIGHT_FRAME_BUFFER_0					0x00010000	// Right Frame Buffer 0
#define __RIGHT_FRAME_BUFFER_1					0x00018000	// Right Frame Buffer 1

#endif
