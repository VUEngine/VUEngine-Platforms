/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <BgmapSprite.h>
#include <CharSet.h>
#ifdef __DEBUG_TOOL
#include <Debug.h>
#endif
#include <DebugConfig.h>
#include <Hardware.h>
#include <Mem.h>
#include <ObjectSprite.h>
#include <Printer.h>
#include <Profiler.h>
#include <Singleton.h>
#include <VirtualList.h>
#include <VirtualNode.h>
#include <VIPSpriteManager.h>
#include <VUEngine.h>

#include <DisplayUnit.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

friend class VirtualNode;
friend class VirtualList;

extern ColumnTableROMSpec DefaultColumnTableSpec;
extern BrightnessRepeatROMSpec DefaultBrightnessRepeatSpec;
extern uint32 _dramDirtyStart;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' MACROS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __TIMEERR					0x8000
#define __XPEND						0x4000
#define __SBHIT						0x2000
#define __FRAMESTART				0x0010
#define __GAMESTART					0x0008
#define __RFBEND					0x0004
#define __LFBEND					0x0002
#define __SCANERR					0x0001

#define __LOCK						0x0400	// VPU SELECT __CTA
#define __SYNCE						0x0200	// L,R_SYNC TO VPU
#define __RE						0x0100	// MEMORY REFLASH CYCLE ON
#define __FCLK						0x0080
#define __SCANRDY					0x0040
#define __DISP						0x0002	// DISPLAY ON
#define __DPRST						0x0001	// RESET VPU COUNTER AND WAIT __FCLK
#define __DPBSY						0x003C	// In the midst of displaying

#define __SBOUT						0x8000					// In FrameBuffer drawing included
#define __SBCOUNT					0x1F00					// Current bloc being drawn
#define __OVERTIME					0x0010					// Processing
#define __XPBSY1					0x0008					// In the midst of FrameBuffer 1 picture editing
#define __XPBSY0					0x0004					// In the midst of FrameBuffer 0 picture editing
#define __XPBSY						(__XPBSY0 | __XPBSY1)  // In the midst of drawing
#define __XPEN						0x0002					// Start of drawing
#define __XPRST						0x0001					// Forcing idling

// VIP Register Mnemonics
#define __INTPND					0x00  // Interrupt Pending
#define __INTENB					0x01  // Interrupt Enable
#define __INTCLR					0x02  // Interrupt Clear

#define __DPSTTS					0x10  // Display Status
#define __DPCTRL					0x11  // Display Control
#define __BRTA						0x12  // Brightness A
#define __BRTB						0x13  // Brightness B
#define __BRTC						0x14  // Brightness C
#define __REST						0x15  // Brightness Idle

#define __FRMCYC					0x17  // Frame Repeat
#define __CTA						0x18  // Column Table Pointer

#define __XPSTTS					0x20  // Drawing Status
#define __XPCTRL					0x21  // Drawing Control
#define __VER						0x22  // VIP Version

#define __GPLT0						0x30  // BGMap Palette 0
#define __GPLT1						0x31  // BGMap Palette 1
#define __GPLT2						0x32  // BGMap Palette 2
#define __GPLT3						0x33  // BGMap Palette 3

#define __JPLT0						0x34  // OBJ Palette 0
#define __JPLT1						0x35  // OBJ Palette 1
#define __JPLT2						0x36  // OBJ Palette 2
#define __JPLT3						0x37  // OBJ Palette 3

#define __BACKGROUND_COLOR			0x38  // Background Color

#define __DIMM_VALUE_1				0x54
#define __DIMM_VALUE_2				0x50

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Processing Effect Registry
/// @memberof DisplayUnit
typedef struct PostProcessingEffectRegistry
{
	PostProcessingEffect postProcessingEffect;
	Entity entity;
	bool remove;

} PostProcessingEffectRegistry;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// VIP registers, accessed externally
volatile uint16* _vipRegisters 						 = (uint16*)0x0005F800;

/// Base address of Column Table (Left Eye)
static uint16* const _columnTableBaseAddressLeft 	__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = (uint16*)0x0003DC00;

/// base address of Column Table (Right Eye)
static uint16* const _columnTableBaseAddressRight 	__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = (uint16*)0x0003DE00; 

/// Linked list of post processing effects to be applied after the VIP's
/// drawing operations are completed
static VirtualList _postProcessingEffects 			__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = NULL;

/// Frame buffers set using during the current game frame
static uint32 _currentDrawingFrameBufferSet 		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = 0;

/// Enum that determines which multiplexed interrupts are allowed
static uint32 _enabledMultiplexedInterrupts 		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = kVIPNoMultiplexedInterrupts;

/// Allows VIP interrupts that the engine doesn't use
static uint16 _customInterrupts 					__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = 0;

/// Register of the interrupts being processed
static uint16 _currentInterrupt 					__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = 0;

/// Time in milliseconds that the game frame last according to the
/// FRMCYC configuration
static uint16 _gameFrameDuration 					__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = __MILLISECONDS_PER_SECOND / __MAXIMUM_FPS;

/// If true, XPEN is raised
static bool _isDrawingAllowed 						__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// If false, no interrupts are enable
static bool _allowInterrupts 						__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// If true, a VIP interrupt happened while in the midst of GAMESTART
static volatile bool _processingGAMESTART 			__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// If true, a VIP interrupt happened while in the midst of XPEND
static volatile bool _processingXPEND 				__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// If true, FRAMESTART happened during XPEND
static volatile bool _FRAMESTARTDuringXPEND 		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// Color related configuration
static DisplayUnitConfig _displayUnitConfig 		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = 
// Display color configuration
{
	// Pointer to the column table's configuration data
	NULL,

	// Color configuration
	{
		// Background color
		__COLOR_BLACK,

		// Brightness
		// These values times the repeat values specified in the column table (max. 16) make the final
		// brightness values on the respective regions of the screen. maximum brightness is 128.
		{
			// Dark red
			__BRIGHTNESS_DARK_RED,
			// Medium red
			__BRIGHTNESS_MEDIUM_RED,
			// Bright red
			__BRIGHTNESS_BRIGHT_RED,
		},

		// Brightness repeat
		(BrightnessRepeatSpec*)NULL,
	},

	// Palettes' configuration
	{
		{
			// Bgmap palette 0
			__BGMAP_PALETTE_0,
			// Bgmap palette 1
			__BGMAP_PALETTE_1,
			// Bgmap palette 2
			__BGMAP_PALETTE_2,
			// Bgmap palette 3
			__BGMAP_PALETTE_3,
		},
		{
			// Object palette 0
			__OBJECT_PALETTE_0,
			// Object palette 1
			__OBJECT_PALETTE_1,
			// Object palette 2
			__OBJECT_PALETTE_2,
			// Object palette 3
			__OBJECT_PALETTE_3,
		},
	}
};

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::interruptHandler()
{
	_currentInterrupt = _vipRegisters[__INTPND];

	// Clear interrupts
	DisplayUnit::clearInterrupts();

	if(kVIPNoMultiplexedInterrupts != _enabledMultiplexedInterrupts)
	{
		if(kVIPOnlyVIPMultiplexedInterrupts == _enabledMultiplexedInterrupts)
		{
			Hardware::setInterruptLevel(_enabledMultiplexedInterrupts);
		}

		Hardware::enableMultiplexedInterrupts();
	}

	// Handle the interrupt
	DisplayUnit::processInterrupt(_currentInterrupt);
	
	if(kVIPNoMultiplexedInterrupts != _enabledMultiplexedInterrupts)
	{
		Hardware::disableMultiplexedInterrupts();

		if(kVIPOnlyVIPMultiplexedInterrupts == _enabledMultiplexedInterrupts)
		{
			Hardware::setInterruptLevel(0);
		}
	}

	_currentInterrupt = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::pushFrontPostProcessingEffect(PostProcessingEffect postProcessingEffect, Entity entity)
{
	PostProcessingEffectRegistry* postProcessingEffectRegistry = 
		DisplayUnit::isPostProcessingEffectRegistered(postProcessingEffect, entity);

	if(!isDeleted(postProcessingEffectRegistry))
	{
		postProcessingEffectRegistry->remove = false;
		return;
	}

	postProcessingEffectRegistry = new PostProcessingEffectRegistry;
	postProcessingEffectRegistry->postProcessingEffect = postProcessingEffect;
	postProcessingEffectRegistry->entity = entity;
	postProcessingEffectRegistry->remove = false;

	VirtualList::pushFront(_postProcessingEffects, postProcessingEffectRegistry);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::pushBackPostProcessingEffect(PostProcessingEffect postProcessingEffect, Entity entity)
{
	PostProcessingEffectRegistry* postProcessingEffectRegistry = 
		DisplayUnit::isPostProcessingEffectRegistered(postProcessingEffect, entity);

	if(!isDeleted(postProcessingEffectRegistry))
	{
		postProcessingEffectRegistry->remove = false;
		return;
	}

	postProcessingEffectRegistry = new PostProcessingEffectRegistry;
	postProcessingEffectRegistry->postProcessingEffect = postProcessingEffect;
	postProcessingEffectRegistry->entity = entity;
	postProcessingEffectRegistry->remove = false;

	VirtualList::pushBack(_postProcessingEffects, postProcessingEffectRegistry);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::removePostProcessingEffect(PostProcessingEffect postProcessingEffect, Entity entity)
{
	for(VirtualNode node = _postProcessingEffects->head; NULL != node; node = node->next)
	{
		PostProcessingEffectRegistry* postProcessingEffectRegistry = (PostProcessingEffectRegistry*)node->data;

		if
		(
			postProcessingEffectRegistry->postProcessingEffect == postProcessingEffect 
			&& 
			postProcessingEffectRegistry->entity == entity
		)
		{
			postProcessingEffectRegistry->remove = true;
			return;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::removePostProcessingEffects()
{
	VirtualList::deleteData(_postProcessingEffects);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::enableCustomInterrupts(uint16 customInterrupts)
{
	_customInterrupts = customInterrupts;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::enableMultiplexedInterrupts(uint32 enabledMultiplexedInterrupts __attribute__((unused)))
{
#ifndef __ENABLE_PROFILER
	_enabledMultiplexedInterrupts = enabledMultiplexedInterrupts;
#else
	_enabledMultiplexedInterrupts = kVIPNoMultiplexedInterrupts;
#endif
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::setFrameCycle(uint8 frameCycle __attribute__((unused)))
{
	if(3 < frameCycle)
	{
		frameCycle = 3;
	}

	_gameFrameDuration = (__MILLISECONDS_PER_SECOND / __MAXIMUM_FPS) << frameCycle;

	_vipRegisters[__FRMCYC] = frameCycle;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::configure(DisplayUnitConfig displayUnitConfig)
{
	_displayUnitConfig = displayUnitConfig;

	DisplayUnit::applyColorConfig(displayUnitConfig.displayColorConfig);
	
	VIPSpriteManager::configure
	(
		VIPSpriteManager::getInstance(), 
		_displayUnitConfig.paramTableSegments, 
		_displayUnitConfig.objectSpritesContainersConfiguration
	);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::applyColorConfig(DisplayColorConfig displayColorConfig)
{
	_displayUnitConfig.displayColorConfig = displayColorConfig;
	
	_vipRegisters[__BACKGROUND_COLOR] = (displayColorConfig.colorConfig.backgroundColor <= __COLOR_BRIGHT_RED)
		? displayColorConfig.colorConfig.backgroundColor
		: __COLOR_BRIGHT_RED;

	_vipRegisters[__GPLT0] = displayColorConfig.paletteConfig.bgmap.gplt0;
	_vipRegisters[__GPLT1] = displayColorConfig.paletteConfig.bgmap.gplt1;
	_vipRegisters[__GPLT2] = displayColorConfig.paletteConfig.bgmap.gplt2;
	_vipRegisters[__GPLT3] = displayColorConfig.paletteConfig.bgmap.gplt3;

	_vipRegisters[__JPLT0] = displayColorConfig.paletteConfig.object.jplt0;
	_vipRegisters[__JPLT1] = displayColorConfig.paletteConfig.object.jplt1;
	_vipRegisters[__JPLT2] = displayColorConfig.paletteConfig.object.jplt2;
	_vipRegisters[__JPLT3] = displayColorConfig.paletteConfig.object.jplt3;

	int32 i, value;

	// Use the default column table as fallback
	if(displayColorConfig.columnTableSpec == NULL)
	{
		displayColorConfig.columnTableSpec = (ColumnTableSpec*)&DefaultColumnTableSpec;
	}

	// Write column table
	for(i = 0; i < 256; i++)
	{
		value = (displayColorConfig.columnTableSpec->mirror && (i > (__COLUMN_TABLE_ENTRIES / 2 - 1)))
			? displayColorConfig.columnTableSpec->columnTable[(__COLUMN_TABLE_ENTRIES - 1) - i]
			: displayColorConfig.columnTableSpec->columnTable[i];

		_columnTableBaseAddressLeft[i] = value;
		_columnTableBaseAddressRight[i] = value;
	}

	// Configure brightness
	while(_isDrawingAllowed && 0 != (_vipRegisters[__XPSTTS] & __XPBSY));
	
	_vipRegisters[__BRTA] = displayColorConfig.colorConfig.brightness.darkRed;
	_vipRegisters[__BRTB] = displayColorConfig.colorConfig.brightness.mediumRed;
	_vipRegisters[__BRTC] = displayColorConfig.colorConfig.brightness.brightRed - 
		(displayColorConfig.colorConfig.brightness.mediumRed + displayColorConfig.colorConfig.brightness.darkRed);

	// Use the default repeat values as fallback
	if(displayColorConfig.colorConfig.brightnessRepeat == NULL)
	{
		displayColorConfig.colorConfig.brightnessRepeat = (BrightnessRepeatSpec*)&DefaultBrightnessRepeatSpec;
	}
	
	// Column table offsets
	int16 leftCta = _vipRegisters[__CTA] & 0xFF;
	int16 rightCta = _vipRegisters[__CTA] >> 8;

	CACHE_RESET;

	// Write repeat values to column table
	for(int16 i = 0; i < 96; i++)
	{
		int16 value = (displayColorConfig.colorConfig.brightnessRepeat->mirror && (i > (__BRIGHTNESS_REPEAT_ENTRIES / 2 - 1)))
			? displayColorConfig.colorConfig.brightnessRepeat->brightnessRepeat[__BRIGHTNESS_REPEAT_ENTRIES - 1 - i] << 8
			: displayColorConfig.colorConfig.brightnessRepeat->brightnessRepeat[i] << 8;

		_columnTableBaseAddressLeft[leftCta - i] = (_columnTableBaseAddressLeft[leftCta - i] & 0xff) | value;
		_columnTableBaseAddressRight[rightCta - i] = (_columnTableBaseAddressRight[rightCta - i] & 0xff) | value;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static DisplayColorConfig DisplayUnit::getColorConfig()
{
	return _displayUnitConfig.displayColorConfig;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static DisplayColorConfig DisplayUnit::getDarkColorConfig()
{
	DisplayColorConfig displayColorConfig = _displayUnitConfig.displayColorConfig;

	displayColorConfig.colorConfig.brightness = (Brightness) {0, 0, 0};
	
	return displayColorConfig;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static int16 DisplayUnit::getBrightnessDirection(DisplayColorConfig targetDisplayColorConfig)
{
	int16 brightRed = 
		targetDisplayColorConfig.colorConfig.brightness.brightRed - _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed;
	int16 mediumRed = 
		targetDisplayColorConfig.colorConfig.brightness.mediumRed - _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed;
	int16 darkRed = 
		targetDisplayColorConfig.colorConfig.brightness.darkRed - _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed;

	return ((darkRed > 0) - (darkRed < 0)) * ((mediumRed > 0) - (mediumRed < 0)) * ((brightRed > 0) - (brightRed < 0));
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool DisplayUnit::modifyBrightness(uint8 amount, DisplayColorConfig targetDisplayColorConfig)
{
	int16 brightRedDelta = 
		targetDisplayColorConfig.colorConfig.brightness.brightRed - _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed;
	int16 mediumRedDelta = 
		targetDisplayColorConfig.colorConfig.brightness.mediumRed - _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed;
	int16 darkRedDelta = 
		targetDisplayColorConfig.colorConfig.brightness.darkRed - _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed;

	// Note: need to cast brightness registers to uint8 because only their lower 8 bits are valid
	bool brightRedDone = 0 == brightRedDelta;
	bool mediumRedDone = 0 == mediumRedDelta;
	bool darkRedDone = 0 == darkRedDelta;

	if(!brightRedDone)
	{
		if(__ABS(brightRedDelta) <=  amount * 4)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed = targetDisplayColorConfig.colorConfig.brightness.brightRed;
			brightRedDone = true;
		}
		else
		{
			brightRedDelta = ((brightRedDelta > 0) - (brightRedDelta < 0)) * amount * 4;
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed += brightRedDelta;
		}
	}

	if(!mediumRedDone)
	{
		if(__ABS(mediumRedDelta) <=  amount * 2)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed = targetDisplayColorConfig.colorConfig.brightness.mediumRed;
			mediumRedDone = true;
		}
		else
		{
			mediumRedDelta = ((mediumRedDelta > 0) - (mediumRedDelta < 0)) * amount * 2;
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed += mediumRedDelta;		
		}
	}

	if(!darkRedDone)
	{
		if(__ABS(darkRedDelta) <=  amount)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed = targetDisplayColorConfig.colorConfig.brightness.darkRed;
			darkRedDone = true;
		}
		else
		{
			darkRedDelta = ((darkRedDelta > 0) - (darkRedDelta < 0)) * amount;
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed += darkRedDelta;		
		}
	}

	_vipRegisters[__BRTA] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed;
	_vipRegisters[__BRTB] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed;
	_vipRegisters[__BRTC] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed;

	return brightRedDone && mediumRedDone && darkRedDone;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool DisplayUnit::upBrightness(uint8 increment)
{
	while(_isDrawingAllowed && 0 != (_vipRegisters[__XPSTTS] & __XPBSY));

	bool darkRedIncreased = true;
	bool mediumRedIncreased = true;
	bool birghtRedIncreased = true;

	if(0 == increment)
	{
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed = 32;
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed = 64;
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed = 32;

		darkRedIncreased = false;
		mediumRedIncreased = false;
		birghtRedIncreased = false;
	}
	else
	{
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed += increment;
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed += increment;
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed += increment;

		if(32 < _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed = 32;
			darkRedIncreased = false;
		}

		if(64 < _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed = 32;
			mediumRedIncreased = false;
		}

		if(32 < _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed = 32;
			birghtRedIncreased = false;
		}
	}

	_vipRegisters[__BRTA] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed;
	_vipRegisters[__BRTB] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed;
	_vipRegisters[__BRTC] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed;

	return darkRedIncreased && mediumRedIncreased && birghtRedIncreased;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool DisplayUnit::lowerBrightness(uint8 decrement)
{
	while(_isDrawingAllowed && 0 != (_vipRegisters[__XPSTTS] & __XPBSY));

	bool darkRedDecreased = true;
	bool mediumRedDecreased = true;
	bool birghtRedDecreased = true;

	if(0 == decrement)
	{
		_vipRegisters[__BRTA] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed = 0;
		_vipRegisters[__BRTB] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed = 0;
		_vipRegisters[__BRTC] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed = 0; 
	}
	else
	{
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed -= decrement;
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed -= decrement;
		_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed -= decrement;

		if(0 > (int8)_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed = 0;
			darkRedDecreased = false;
		}

		if(0 > (int8)_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed = 0;
			mediumRedDecreased = false;
		}

		if(0 > (int8)_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed)
		{
			_displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed = 0;
			birghtRedDecreased = false;
		}
	}

	_vipRegisters[__BRTA] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.darkRed;
	_vipRegisters[__BRTB] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.mediumRed;
	_vipRegisters[__BRTC] = _displayUnitConfig.displayColorConfig.colorConfig.brightness.brightRed;

	return darkRedDecreased && mediumRedDecreased && birghtRedDecreased;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::showException()
{
	_displayUnitConfig.displayColorConfig.colorConfig.brightness = (Brightness) {32, 64, 32};
	_displayUnitConfig.displayColorConfig.colorConfig.backgroundColor = __COLOR_BLACK;
	_displayUnitConfig.displayColorConfig.paletteConfig = (PaletteConfig)
	{
		{0xE4, __DIMM_VALUE_2, __DIMM_VALUE_2, __DIMM_VALUE_2},
		{__DIMM_VALUE_2, __DIMM_VALUE_2, __DIMM_VALUE_2, __DIMM_VALUE_2}
	};

	DisplayUnit::applyColorConfig(_displayUnitConfig.displayColorConfig);

	// Error display message
	WorldAttributes* worldAttributesBaseAddress = (WorldAttributes*)__WORLD_SPACE_BASE_ADDRESS;

	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].mx = Printer::getPrintingBgmapXOffset();
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].mp = __PRINTING_BGMAP_PARALLAX_OFFSET;
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].my = Printer::getPrintingBgmapYOffset();
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].gx = 0;
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].gp = 0;
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].gy = 0;
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].w = __SCREEN_WIDTH;
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].h = __SCREEN_HEIGHT;
	worldAttributesBaseAddress[__EXCEPTIONS_WORLD].head = 
		__WORLD_ON | __WORLD_BGMAP | __WORLD_OVR | Printer::getPrintingBgmapSegment();

	worldAttributesBaseAddress[__EXCEPTIONS_WORLD - 1].head = __WORLD_END;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

uint16 DisplayUnit::getCurrentInterrupt()
{
	return _currentInterrupt;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

uint16 DisplayUnit::getGameFrameDuration()
{
	return _gameFrameDuration;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::reset()
{
	DisplayUnit::getInstance();
	
	if(NULL == _postProcessingEffects)
	{
		_postProcessingEffects = new VirtualList();
	}
	
	_currentDrawingFrameBufferSet = 0;
	_FRAMESTARTDuringXPEND = false;
	_customInterrupts = 0;
	_currentInterrupt = 0;
	_processingGAMESTART = false;
	_processingXPEND = false;
	_isDrawingAllowed = false;
	_enabledMultiplexedInterrupts = kVIPNoMultiplexedInterrupts;
	_allowInterrupts = true;
 
	DisplayUnit::lowerBrightness(0);
	DisplayUnit::removePostProcessingEffects();

	DisplayUnit::setFrameCycle(__FRAME_CYCLE);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::clearGraphicMemory()
{
	WorldAttributes* worldAttributesBaseAddress = (WorldAttributes*)__WORLD_SPACE_BASE_ADDRESS;
	ObjectAttributes* objectAttributesBaseAddress = (ObjectAttributes*)__OBJECT_SPACE_BASE_ADDRESS;

	for(int32 i = 0; i < __TOTAL_WORLD_LAYERS; i++)
	{
		_worldAttributesCache[i].head = 0;
		_worldAttributesCache[i].gx = 0;
		_worldAttributesCache[i].gp = 0;
		_worldAttributesCache[i].gy = 0;
		_worldAttributesCache[i].mx = 0;
		_worldAttributesCache[i].mp = 0;
		_worldAttributesCache[i].my = 0;
		_worldAttributesCache[i].w = 0;
		_worldAttributesCache[i].h = 0;
		_worldAttributesCache[i].param = 0;
		_worldAttributesCache[i].ovr = 0;

		worldAttributesBaseAddress[i].head = 0;
		worldAttributesBaseAddress[i].gx = 0;
		worldAttributesBaseAddress[i].gp = 0;
		worldAttributesBaseAddress[i].gy = 0;
		worldAttributesBaseAddress[i].mx = 0;
		worldAttributesBaseAddress[i].mp = 0;
		worldAttributesBaseAddress[i].my = 0;
		worldAttributesBaseAddress[i].w = 0;
		worldAttributesBaseAddress[i].h = 0;
		worldAttributesBaseAddress[i].param = 0;
		worldAttributesBaseAddress[i].ovr = 0;
	}

	for(int32 i = 0; i < __TOTAL_OBJECTS; i++)
	{
		_objectAttributesCache[i].jx = 0;
		_objectAttributesCache[i].head = 0;
		_objectAttributesCache[i].jy = 0;
		_objectAttributesCache[i].tile = 0;

		objectAttributesBaseAddress[i].jx = 0;
		objectAttributesBaseAddress[i].head = 0;
		objectAttributesBaseAddress[i].jy = 0;
		objectAttributesBaseAddress[i].tile = 0;
	}

	for(int32 i = __TOTAL_OBJECTS - 1; 0 <= i; i--)
	{
		_objectAttributesCache[i].head = __OBJECT_SPRITE_CHAR_HIDE_MASK;
	}

	for(int32 i = 0; i < __TOTAL_OBJECTS; i++)
	{
		_vipRegisters[__SPT3 - i] = 0;
		_objectAttributesCache[i].jx = 0;
		_objectAttributesCache[i].head = 0;
		_objectAttributesCache[i].jy = 0;
		_objectAttributesCache[i].tile = 0;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::startMemoryRefresh()
{
	_vipRegisters[__FRMCYC] = 0;
	_vipRegisters[__DPCTRL] = _vipRegisters[__DPSTTS] | (__SYNCE | __RE);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::waitForFrame()
{
	while(!(_vipRegisters[__DPSTTS] & __FCLK));
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::startDrawing()
{
	_isDrawingAllowed = true;

	DisplayUnit::enableInterrupts(__GAMESTART | __XPEND);

	while(0 != (_vipRegisters[__XPSTTS] & __XPBSY));
	
	_vipRegisters[__XPCTRL] |= __XPEN;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::resumeDrawing()
{
	if(_isDrawingAllowed)
	{
		while(0 != (_vipRegisters[__XPSTTS] & __XPBSY));

		_vipRegisters[__XPCTRL] |= __XPEN;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::suspendDrawing()
{
	_isDrawingAllowed = DisplayUnit::isDrawingAllowed();
	
	while(_isDrawingAllowed && 0 != (_vipRegisters[__XPSTTS] & __XPBSY));

	_vipRegisters[__XPCTRL] &= ~__XPEN;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::stopDrawing()
{
	DisplayUnit::disableInterrupts();

	while(_isDrawingAllowed && 0 != (_vipRegisters[__XPSTTS] & __XPBSY));

	_vipRegisters[__XPCTRL] &= ~__XPEN;

	_isDrawingAllowed = false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::startDisplaying()
{
	_vipRegisters[__REST] = 0;
	_vipRegisters[__DPCTRL] = (__SYNCE | __RE | __DISP) & ~__LOCK;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::stopDisplaying()
{
	_vipRegisters[__REST] = 0;
	_vipRegisters[__DPCTRL] = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::startRendering()
{
	VIPSpriteManager::startRendering(VIPSpriteManager::getInstance());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::stopRendering()
{
	VIPSpriteManager::stopRendering(VIPSpriteManager::getInstance());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::disableRendering()
{
	VIPSpriteManager::disableRendering(VIPSpriteManager::getInstance());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::commitGraphics()
{
	VIPSpriteManager::commitGraphics(VIPSpriteManager::getInstance());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::fillAvailableSlots(int16* availableSlots,  const int16** nextSlotIndex, int16 totalSpriteLists __attribute__((unused)))
{
	VIPSpriteManager::fillAvailableSlots(VIPSpriteManager::getInstance(), availableSlots,  nextSlotIndex, totalSpriteLists);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 DisplayUnit::getSpriteListIndex(Sprite sprite)
{
	return VIPSpriteManager::getSpriteListIndex(VIPSpriteManager::getInstance(), sprite);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::allowInterrupts(bool allowInterrupts)
{
	_allowInterrupts = allowInterrupts;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 DisplayUnit::getCurrentDrawingFrameBufferSet()
{
	return _currentDrawingFrameBufferSet;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::print(int16 x, int16 y)
{
	int16 xDisplacement = 8;

	Printer::text("VIP\nRegisters", x, ++y, NULL);
	y += 2;
	Printer::text("INTPND:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__INTPND], x + xDisplacement, y, 4, NULL);
	Printer::text("INTENB:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__INTENB], x + xDisplacement, y, 4, NULL);
	Printer::text("INTCLR:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__INTCLR], x + xDisplacement, y, 4, NULL);
	Printer::text("DPSTTS:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__DPSTTS], x + xDisplacement, y, 4, NULL);
	Printer::text("DPCTRL:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__DPCTRL], x + xDisplacement, y, 4, NULL);
	Printer::text("BRTA:", x, ++y, NULL);
	Printer::hex((uint8)_vipRegisters[__BRTA], x + xDisplacement, y, 4, NULL);
	Printer::text("BRTB:", x, ++y, NULL);
	Printer::hex((uint8)_vipRegisters[__BRTB], x + xDisplacement, y, 4, NULL);
	Printer::text("BRTC:", x, ++y, NULL);
	Printer::hex((uint8)_vipRegisters[__BRTC], x + xDisplacement, y, 4, NULL);
	Printer::text("REST:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__REST], x + xDisplacement, y, 4, NULL);
	Printer::text("FRMCYC:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__FRMCYC], x + xDisplacement, y, 4, NULL);
	Printer::text("CTA:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__CTA], x + xDisplacement, y, 4, NULL);
	Printer::text("XPSTTS:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__XPSTTS], x + xDisplacement, y, 4, NULL);
	Printer::text("XPCTRL:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__XPCTRL], x + xDisplacement, y, 4, NULL);
	Printer::text("VER:", x, ++y, NULL);
	Printer::hex(_vipRegisters[__VER], x + xDisplacement, y, 4, NULL);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————


static void DisplayUnit::processInterrupt(uint16 interrupt)
{
	static uint16 interruptTable[] =
	{
		__FRAMESTART,
		__GAMESTART,
		__XPEND,
#ifndef __SHIPPING
		__TIMEERR,
		__SCANERR
#endif
	};

	for(uint32 i = 0; i < sizeof(interruptTable) / sizeof(uint16); i++)
	{
		switch(interrupt & interruptTable[i])
		{
			case __FRAMESTART:
			{
				_FRAMESTARTDuringXPEND = _processingXPEND;

				DisplayUnit::fireEvent(DisplayUnit::getInstance(), kEventDisplayUnitFrameStart);
				break;
			}

			case __GAMESTART:
			{
				_processingGAMESTART = true;

				DisplayUnit::registerCurrentDrawingFrameBufferSet();

				DisplayUnit::fireEvent
				(
					DisplayUnit::getInstance(), 
					_processingXPEND ? 
						kEventDisplayUnitGameStartDuringVBlank 
						:
						kEventDisplayUnitGameStart
				);

				_processingGAMESTART = false;

				break;
			}

			case __XPEND:
			{
#ifdef __ENABLE_PROFILER
				Profiler::lap(kProfilerLapTypeStartInterrupt, NULL);
#endif
				DisplayUnit::suspendDrawing();

				_processingXPEND = true;				

				DisplayUnit::fireEvent
				(
					DisplayUnit::getInstance(), 
					_processingGAMESTART ? 
						kEventDisplayUnitVBlankDuringGameStart 
						:
						kEventDisplayUnitVBlank
				);

				DisplayUnit::applyPostProcessingEffects();

				_processingXPEND = false;

				DisplayUnit::resumeDrawing();

#ifdef __ENABLE_PROFILER
				Profiler::lap(kProfilerLapTypeVIPInterruptXPENDProcess, PROCESS_NAME_VRAM_WRITE);
#endif
				break;
			}
#ifndef __SHIPPING
			case __TIMEERR:
			{
				DisplayUnit::fireEvent(DisplayUnit::getInstance(), kEventDisplayUnitTimeError);
				break;
			}

			case __SCANERR:
			{
				DisplayUnit::fireEvent(DisplayUnit::getInstance(), kEventDisplayUnitScanError);

				Error::triggerException("DisplayUnit::servo error", NULL);
				break;
			}
#endif
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::enableInterrupts(uint16 interruptCode)
{
	if(!_allowInterrupts)
	{
		return;
	}

	_vipRegisters[__INTCLR] = _vipRegisters[__INTPND];

	interruptCode |= _customInterrupts;

#ifndef __SHIPPING
	_vipRegisters[__INTENB] = interruptCode | __FRAMESTART | __TIMEERR | __SCANERR;
#else
	_vipRegisters[__INTENB] = interruptCode | __FRAMESTART;
#endif
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::disableInterrupts()
{
	_vipRegisters[__INTENB] = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::clearInterrupts()
{
	_vipRegisters[__INTCLR] = _vipRegisters[__INTPND];
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::applyPostProcessingEffects()
{
	for
	(
		VirtualNode node = _postProcessingEffects->tail, previousNode = NULL; 
		!_FRAMESTARTDuringXPEND && NULL != node; 
		node = previousNode
	)
	{
		previousNode = node->previous;

		PostProcessingEffectRegistry* postProcessingEffectRegistry = (PostProcessingEffectRegistry*)node->data;

		if(isDeleted(postProcessingEffectRegistry) || postProcessingEffectRegistry->remove)
		{
			VirtualList::removeNode(_postProcessingEffects, node);

			if(!isDeleted(postProcessingEffectRegistry))
			{
				delete postProcessingEffectRegistry;
			}
		}
		else
		{
			postProcessingEffectRegistry->postProcessingEffect(postProcessingEffectRegistry->entity);
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::useInternalColumnTable(bool useInternal)
{
	if(useInternal)
	{
		// Set lock bit
		_vipRegisters[__DPCTRL] |= __LOCK;
	}
	else
	{
		// Unset lock bit
		_vipRegisters[__DPCTRL] &= ~__LOCK;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void DisplayUnit::registerCurrentDrawingFrameBufferSet()
{
	uint16 currentDrawingFrameBufferSet = _vipRegisters[__XPSTTS] & __XPBSY;

	if(0x0004 == currentDrawingFrameBufferSet)
	{
		_currentDrawingFrameBufferSet = 0;
	}
	else if(0x0008 == currentDrawingFrameBufferSet)
	{
		_currentDrawingFrameBufferSet = 0x8000;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool DisplayUnit::isDrawingAllowed()
{
	return 0 != (_vipRegisters[__XPSTTS] & __XPEN);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static PostProcessingEffectRegistry* DisplayUnit::isPostProcessingEffectRegistered
(
	PostProcessingEffect postProcessingEffect, Entity entity
)
{
	VirtualNode node = _postProcessingEffects->head;

	for(; NULL != node; node = node->next)
	{
		PostProcessingEffectRegistry* postProcessingEffectRegistry = (PostProcessingEffectRegistry*)node->data;

		if
		(
			postProcessingEffectRegistry->postProcessingEffect == postProcessingEffect 
			&& 
			postProcessingEffectRegistry->entity == entity
		)
		{
			return postProcessingEffectRegistry;
		}
	}

	return NULL;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static int16 DisplayUnit::getCurrentBlockBeingDrawn()
{
	if(_vipRegisters[__XPSTTS] & __SBOUT)
	{
		return (_vipRegisters[__XPSTTS] & __SBCOUNT) >> 8;
	}

	if(0 == (_vipRegisters[__XPSTTS] & __XPBSY))
	{
		return -1;
	}

	while(!(_vipRegisters[__XPSTTS] & __SBOUT));

	return (_vipRegisters[__XPSTTS] & __SBCOUNT) >> 8;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void DisplayUnit::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void DisplayUnit::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
