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

#include <DebugConfig.h>
#include <DisplayUnit.h>
#include <Singleton.h>
#include <Telegram.h>

#include "ColumnTable.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

extern volatile uint16* const _vipRegisters;
extern ColumnTableROMSpec DefaultColumnTableSpec;
extern BrightnessRepeatROMSpec DefaultBrightnessRepeatSpec;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' MACROS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// These are facts of the hardware, not tunables

/// Number of groups of columns that make up one eye's display
#define __COLUMN_TABLE_GROUPS					96

/// Number of columns that share a single column table entry
#define __COLUMN_TABLE_COLUMNS_PER_GROUP		4

/// Bits used to split a group: a group spans 1 << bits position units, and the time
/// it is displayed is also split in as many parts
#define __COLUMN_TABLE_FRACTION_BITS			10
#define __COLUMN_TABLE_GROUP_UNITS				(1 << __COLUMN_TABLE_FRACTION_BITS)

/// Groups beyond the right edge that a displaced position is allowed to reach
#define __COLUMN_TABLE_OVERSHOOT_GROUPS			24
#define __COLUMN_TABLE_REACH					(__COLUMN_TABLE_GROUPS + __COLUMN_TABLE_OVERSHOOT_GROUPS)

/// A group's time is columns per group times 1 << fraction bits, so dividing by it with
/// rounding turns a time into a column length
#define __COLUMN_TABLE_GROUP_TIME_SHIFT			(__COLUMN_TABLE_FRACTION_BITS + 2)

/// Longest length a column table entry can hold (it is stored minus one)
#define __COLUMN_TABLE_MAXIMUM_LENGTH			256

/// Fixed point scales: strength 256 means 1.0, profiles of 16384 mean 1.0
#define __COLUMN_TABLE_STRENGTH_SHIFT			8
#define __COLUMN_TABLE_PROFILE_SHIFT			14

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

typedef struct ColumnTableCalibration
{
	// Nintendo's table, or the game's own
	const ColumnTableSpec* reference;

	// Shortest length that displays cleanly
	uint8 minimumLength;

	// Groups per eye (96)
	uint8 groups;

} ColumnTableCalibration;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Calibration used unless the game sets its own
const ColumnTableCalibration _defaultColumnTableCalibration =
{
	// Reference table. NULL: the one the display unit is currently configured with
	NULL,

	// Shortest column length that displays cleanly on the measured hardware
	0x30,

	// Groups per eye
	96
};

/// The effect being played: the curves over time and the profiles across the screen.
static const ColumnTableEffectSpec* _columnTableEffectSpec 	= NULL;

/// Hardware calibration the lengths are computed against
static const ColumnTableCalibration* _columnTableCalibration = &_defaultColumnTableCalibration;

/// Number of frames played since the effect started. It selects the current entry
/// of the spec's curves and is advanced once per displayed frame
static uint16 _age											= 0;

/// True while the effect is playing and rewriting the column tables. When it turns false,
/// the tables are back to their normal lengths and nothing is rebuilt. Written from both
/// the game loop and the interrupt handler
static volatile bool _active								= false;

/// True while this class is registered with the display unit and its interrupts are enabled
static bool _listening										= false;

/// True once the tables of the coming frame are computed and waiting to be written
static bool _tablesBuilt									= false;

/// Number of groups, among the visible ones, whose length had to be limited in the last
/// frame because the effect asked for more than the hardware can do
static uint16 _clampedGroups								= 0;

/// Column table pointer at the start of each eye's display
static uint8 _startLeft										= 0;
static uint8 _startRight									= 0;

/// Base address of the column tables in VIP memory
static uint16* const _columnTableBaseAddressLeft 			= (uint16*)0x0003DC00;
static uint16* const _columnTableBaseAddressRight 			= (uint16*)0x0003DE00;

/// Column lengths of the reference table, all entries, with the mirror already applied
static uint8 _referenceLengths[__COLUMN_TABLE_ENTRIES];

/// Time at which the reference table reaches each group edge, in units of 200 ns
static int32 _referenceTimes[__COLUMN_TABLE_REACH + 1];

/// Brightness repeat of each group, taken from the display unit's current configuration
static uint8 _repeats[__COLUMN_TABLE_GROUPS];

/// Where the left edge of each group should land for each eye, in 1/256 of a pixel.
/// The extra entry is the right edge of the last group
static int32 _positionsLeft[__COLUMN_TABLE_GROUPS + 1];
static int32 _positionsRight[__COLUMN_TABLE_GROUPS + 1];

/// Column tables computed for the coming frame, written to the VIP once both eyes are displayed
static uint16 _tableLeft[__COLUMN_TABLE_ENTRIES];
static uint16 _tableRight[__COLUMN_TABLE_ENTRIES];

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::reset()
{
	_active = false;
	_tablesBuilt = false;
	_columnTableEffectSpec = NULL;
	_columnTableCalibration = &_defaultColumnTableCalibration;
	_age = 0;
	_clampedGroups = 0;

	ColumnTable::stopListening();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::setCalibration(const ColumnTableCalibration* columnTableCalibration)
{
	// It is picked up by the next effect started
	_columnTableCalibration = NULL != columnTableCalibration ? columnTableCalibration : &_defaultColumnTableCalibration;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::startEffect(const ColumnTableEffectSpec* columnTableEffectSpec)
{
	if(NULL == columnTableEffectSpec)
	{
		return;
	}

	// The interrupt handler must never see an effect that is half set up
	_active = false;
	_tablesBuilt = false;

	_columnTableEffectSpec = columnTableEffectSpec;
	_age = 0;
	_clampedGroups = 0;

	ColumnTable::loadReference();
	ColumnTable::measureStarts();
	ColumnTable::startListening();

	_active = true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::stopEffect(const ColumnTableEffectSpec* columnTableEffectSpec)
{
	if(NULL == columnTableEffectSpec || _columnTableEffectSpec == columnTableEffectSpec)
	{
		ColumnTable::reset();
	}

	// Put back the normal lengths and repeats
	DisplayUnit::applyColorConfig(DisplayUnit::getColorConfig());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool ColumnTable::isActive()
{
	return _active;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 ColumnTable::getClampedGroups()
{
	return _clampedGroups;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::measureStarts()
{
	uint16 columnTablePointer = _vipRegisters[__CTA];

	_startLeft = (uint8)(columnTablePointer & 0xFF);
	_startRight = (uint8)(columnTablePointer >> 8);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool ColumnTable::onEvent(ListenerObject eventFirer __attribute__((unused)), uint16 eventCode)
{
	if(!_active)
	{
		return true;
	}

	switch(eventCode)
	{
		case kEventDisplayUnitLeftFrameBufferEnd:

			// The right eye is being displayed: compute into RAM, the VIP is not touched yet
			ColumnTable::evaluate();
			ColumnTable::buildTables();
			break;

		case kEventDisplayUnitRightFrameBufferEnd:

			// Both eyes are done: write the tables before the next left eye starts
			ColumnTable::commit();
			break;
	}

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::startListening()
{
	if(_listening)
	{
		return;
	}

	ColumnTable columnTable = ColumnTable::getInstance();
	DisplayUnit displayUnit = DisplayUnit::getInstance();

	DisplayUnit::addEventListener
	(
		displayUnit, ListenerObject::safeCast(columnTable), kEventDisplayUnitLeftFrameBufferEnd
	);

	DisplayUnit::addEventListener
	(
		displayUnit, ListenerObject::safeCast(columnTable), kEventDisplayUnitRightFrameBufferEnd
	);

	// The mask only reaches the VIP when the interrupts are enabled again
	DisplayUnit::enableCustomInterrupts(__LFBEND | __RFBEND);
	DisplayUnit::resumeInterrupts();

	_listening = true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::stopListening()
{
	if(!_listening)
	{
		return;
	}

	_listening = false;

	ColumnTable columnTable = ColumnTable::getInstance();
	DisplayUnit displayUnit = DisplayUnit::getInstance();

	DisplayUnit::removeEventListener
	(
		displayUnit, ListenerObject::safeCast(columnTable), kEventDisplayUnitLeftFrameBufferEnd
	);

	DisplayUnit::removeEventListener
	(
		displayUnit, ListenerObject::safeCast(columnTable), kEventDisplayUnitRightFrameBufferEnd
	);

	DisplayUnit::enableCustomInterrupts(0);
	DisplayUnit::resumeInterrupts();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::loadReference()
{
	const ColumnTableSpec* columnTableSpec = _columnTableCalibration->reference;

	if(NULL == columnTableSpec)
	{
		columnTableSpec = DisplayUnit::getColorConfig().columnTableSpec;
	}

	if(NULL == columnTableSpec)
	{
		columnTableSpec = (ColumnTableSpec*)&DefaultColumnTableSpec;
	}

	for(int32 i = 0; i < __COLUMN_TABLE_ENTRIES; i++)
	{
		// Same lookup the display unit uses when it writes the table
		_referenceLengths[i] = (uint8)
		(
			(columnTableSpec->mirror && (i > (__COLUMN_TABLE_ENTRIES / 2 - 1)))
				? columnTableSpec->columnTable[(__COLUMN_TABLE_ENTRIES - 1) - i]
				: columnTableSpec->columnTable[i]
		);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::loadRepeats()
{
	const BrightnessRepeatSpec* brightnessRepeatSpec = DisplayUnit::getColorConfig().colorConfig.brightnessRepeat;

	if(NULL == brightnessRepeatSpec)
	{
		brightnessRepeatSpec = (BrightnessRepeatSpec*)&DefaultBrightnessRepeatSpec;
	}

	for(int32 group = 0; group < __COLUMN_TABLE_GROUPS; group++)
	{
		_repeats[group] = (uint8)
		(
			(brightnessRepeatSpec->mirror && (group > (__BRIGHTNESS_REPEAT_ENTRIES / 2 - 1)))
				? brightnessRepeatSpec->brightnessRepeat[__BRIGHTNESS_REPEAT_ENTRIES - 1 - group]
				: brightnessRepeatSpec->brightnessRepeat[group]
		);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::evaluate()
{
	const ColumnTableEffectSpec* spec = _columnTableEffectSpec;

	int32 shiftAmount = 0;
	int32 depthAmount = 0;

	// Past the last frame both amounts stay at zero, and zero is the plain table
	if(_age < spec->frames)
	{
		shiftAmount = (spec->shiftCurve[_age] * spec->strength) >> __COLUMN_TABLE_STRENGTH_SHIFT;
		depthAmount = (spec->depthCurve[_age] * spec->strength) >> __COLUMN_TABLE_STRENGTH_SHIFT;
	}

	for(int32 group = 0; group <= __COLUMN_TABLE_GROUPS; group++)
	{
		int32 shift = (shiftAmount * spec->shiftProfile[group]) >> __COLUMN_TABLE_PROFILE_SHIFT;
		int32 depth = (depthAmount * spec->depthProfile[group]) >> __COLUMN_TABLE_PROFILE_SHIFT;

		int32 at = group * __COLUMN_TABLE_GROUP_UNITS + shift;

		// Disparity is right minus left and is split between the eyes
		_positionsLeft[group] = at - (depth >> 1);
		_positionsRight[group] = at + depth - (depth >> 1);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::buildTables()
{
	_clampedGroups = 0;

	ColumnTable::loadRepeats();
	ColumnTable::buildEye(_tableLeft, _positionsLeft, _startLeft);
	ColumnTable::buildEye(_tableRight, _positionsRight, _startRight);

	_tablesBuilt = true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::buildEye(uint16* table, const int32* positions, uint8 start)
{
	// Entries nobody displays keep the reference lengths
	for(int32 i = 0; i < __COLUMN_TABLE_ENTRIES; i++)
	{
		table[i] = _referenceLengths[i];
	}

	// Time at which the reference table reaches each group edge. Group k of an eye uses
	// the entry start - k
	_referenceTimes[0] = 0;

	for(int32 group = 0; group < __COLUMN_TABLE_REACH; group++)
	{
		_referenceTimes[group + 1] = _referenceTimes[group] 
			+ __COLUMN_TABLE_COLUMNS_PER_GROUP * (_referenceLengths[(uint8)(start - group)] + 1);
	}

	int32 minimumLength = _columnTableCalibration->minimumLength + 1;
	int32 shownEdge = __COLUMN_TABLE_GROUPS * __COLUMN_TABLE_GROUP_UNITS;
	int32 elapsed = 0;

	for(int32 group = 0; group < __COLUMN_TABLE_GROUPS; group++)
	{
		// The left edge cannot move, so everything is measured from where it is
		int32 target = ColumnTable::timeAt(start, positions[group + 1] - positions[0]);

		// Time this group needs to end where the target says, as a length rounded to the
		// nearest, so that the error never adds up across the screen
		int32 length = ((target - elapsed) + (1 << (__COLUMN_TABLE_GROUP_TIME_SHIFT - 1))) 
			>> __COLUMN_TABLE_GROUP_TIME_SHIFT;

		// Groups pushed past the right edge are given the least time on purpose, so
		// only the visible ones count as clamped
		bool visible = positions[group] - positions[0] < shownEdge;

		if(length < minimumLength)
		{
			length = minimumLength;
			_clampedGroups += visible;
		}
		else if(__COLUMN_TABLE_MAXIMUM_LENGTH < length)
		{
			length = __COLUMN_TABLE_MAXIMUM_LENGTH;
			_clampedGroups += visible;
		}

		elapsed += length << __COLUMN_TABLE_GROUP_TIME_SHIFT;

		table[(uint8)(start - group)] = (uint16)((_repeats[group] << 8) | (length - 1));
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static int32 ColumnTable::timeAt(uint8 start, int32 position)
{
	if(position <= 0)
	{
		return 0;
	}

	int32 group = position >> __COLUMN_TABLE_FRACTION_BITS;

	if(__COLUMN_TABLE_REACH <= group)
	{
		return _referenceTimes[__COLUMN_TABLE_REACH] << __COLUMN_TABLE_FRACTION_BITS;
	}

	// The reference table is calibrated so that columns are evenly spaced, so a position
	// a fraction into a group is reached that fraction into the group's time
	return (_referenceTimes[group] << __COLUMN_TABLE_FRACTION_BITS) 
		+ __COLUMN_TABLE_COLUMNS_PER_GROUP 
			* (_referenceLengths[(uint8)(start - group)] + 1) 
			* (position & (__COLUMN_TABLE_GROUP_UNITS - 1));
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::commit()
{
	// A frame that started after the left eye's computation has nothing to write
	if(!_tablesBuilt)
	{
		return;
	}

	for(int32 i = 0; i < __COLUMN_TABLE_ENTRIES; i++)
	{
		_columnTableBaseAddressLeft[i] = _tableLeft[i];
		_columnTableBaseAddressRight[i] = _tableRight[i];
	}

	_tablesBuilt = false;

	// The frame after the last one of the curves is built flat, so the tables end up
	// as the normal ones before the effect lets go
	if(_columnTableEffectSpec->frames <= _age)
	{
		_active = false;
		return;
	}

	_age++;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ColumnTable::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ColumnTable::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
