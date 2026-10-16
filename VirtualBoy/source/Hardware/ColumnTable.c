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


/// Calibration used unless the game sets its own
const ColumnTableCalibration DefaultColumnTableCalibration =
{
	// Reference table. NULL: the one the display unit is currently configured with
	NULL,

	// Shortest column length that displays cleanly on the measured hardware
	0x30,

	// Groups per eye
	96
};

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// The effect being played: the curves over time and the profiles across the screen.
static const ColumnTableEffectSpec* _columnTableEffectSpec 	= NULL;

/// Hardware calibration the lengths are computed against
static const ColumnTableCalibration* _columnTableCalibration = &DefaultColumnTableCalibration;

/// Table the lengths are measured against. It is read in place, never copied, so it has
/// to stay in memory while an effect plays
static const ColumnTableSpec* _referenceColumnTableSpec 	= NULL;

/// Number of frames played since the effect started. It selects the current entry
/// of the spec's curves and is advanced once per displayed frame
static uint16 _age											= 0;

/// True once the lengths of the coming frame are computed and waiting to be written
static bool _tablesBuilt									= false;

/// Base address of the column tables in VIP memory
static uint16* const _columnTableBaseAddressLeft 			= (uint16*)0x0003DC00;
static uint16* const _columnTableBaseAddressRight 			= (uint16*)0x0003DE00;

/// Position in the reference table that the time lookups walk from: the group it stands on
/// and the time at which the reference table reaches that group's left edge, in units of 200 ns.
/// It starts over for each eye and only moves as far as positions are displaced
static int32 _cursorGroup									= 0;
static int32 _cursorTime									= 0;

/// Column length minus one of every group, computed for the coming frame and written to the
/// VIP once both eyes are displayed. Entries the display never reaches are left alone
static uint8 _lengthsLeft[__COLUMN_TABLE_GROUPS];
static uint8 _lengthsRight[__COLUMN_TABLE_GROUPS];

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::reset()
{
	_tablesBuilt = false;
	_columnTableEffectSpec = NULL;
	_columnTableCalibration = &DefaultColumnTableCalibration;
	_referenceColumnTableSpec = NULL;
	_age = 0;

	DisplayUnit::removeEventListener
	(
		DisplayUnit::getInstance(), ListenerObject::safeCast(ColumnTable::getInstance()), kEventDisplayUnitLeftFrameBufferEnd
	);

	DisplayUnit::removeEventListener
	(
		DisplayUnit::getInstance(), ListenerObject::safeCast(ColumnTable::getInstance()), kEventDisplayUnitRightFrameBufferEnd
	);

	DisplayUnit::enableCustomInterrupts(0);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::setCalibration(const ColumnTableCalibration* columnTableCalibration)
{
	// It is picked up by the next effect started
	_columnTableCalibration = NULL != columnTableCalibration ? columnTableCalibration : &DefaultColumnTableCalibration;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::startEffect(const ColumnTableEffectSpec* columnTableEffectSpec)
{
	if(NULL == columnTableEffectSpec)
	{
		return;
	}

	_tablesBuilt = false;

	_columnTableEffectSpec = columnTableEffectSpec;
	_age = 0;

	ColumnTable::loadReference();

	DisplayUnit::addEventListener
	(
		DisplayUnit::getInstance(), ListenerObject::safeCast(ColumnTable::getInstance()), kEventDisplayUnitLeftFrameBufferEnd
	);

	DisplayUnit::addEventListener
	(
		DisplayUnit::getInstance(), ListenerObject::safeCast(ColumnTable::getInstance()), kEventDisplayUnitRightFrameBufferEnd
	);

	// The mask only reaches the VIP when the interrupts are enabled again
	DisplayUnit::enableCustomInterrupts(__LFBEND | __RFBEND);
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
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool ColumnTable::onEvent(ListenerObject eventFirer __attribute__((unused)), uint16 eventCode)
{
	switch(eventCode)
	{
		case kEventDisplayUnitLeftFrameBufferEnd:

			// The right eye is being displayed: compute into RAM, the VIP is not touched yet
			return !ColumnTable::buildTables();
			break;

		case kEventDisplayUnitRightFrameBufferEnd:

			// Both eyes are done: write the lengths before the next left eye starts
			return !ColumnTable::commit();
			break;
	}

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::loadReference()
{
	if(NULL == _columnTableCalibration)
	{
		return;
	}
	
	const ColumnTableSpec* columnTableSpec = _columnTableCalibration->reference;

	if(NULL == columnTableSpec)
	{
		columnTableSpec = DisplayUnit::getColorConfig().columnTableSpec;
	}

	if(NULL == columnTableSpec)
	{
		columnTableSpec = (ColumnTableSpec*)&DefaultColumnTableSpec;
	}

	_referenceColumnTableSpec = columnTableSpec;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint8 ColumnTable::getReferenceLength(uint8 index)
{
	// Same lookup the display unit uses when it writes the table
	return (uint8)
	(
		(_referenceColumnTableSpec->mirror && (index > ((__COLUMN_TABLE_ENTRIES >> 1) - 1)))
			? _referenceColumnTableSpec->columnTable[(__COLUMN_TABLE_ENTRIES - 1) - index]
			: _referenceColumnTableSpec->columnTable[index]
	);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static int32 ColumnTable::getPosition(int32 group, bool rightEye, int32 shiftAmount, int32 depthAmount)
{
	int32 shift = (shiftAmount * _columnTableEffectSpec->shiftProfile[group]) >> __COLUMN_TABLE_PROFILE_SHIFT;
	int32 depth = (depthAmount * _columnTableEffectSpec->depthProfile[group]) >> __COLUMN_TABLE_PROFILE_SHIFT;

	int32 at = group * __COLUMN_TABLE_GROUP_UNITS + shift;

	// Disparity is right minus left and is split between the eyes
	return rightEye ? at + depth - (depth >> 1) : at - (depth >> 1);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static inline uint8 ColumnTable::getStartLeft()
{
	return (uint8)(_vipRegisters[__CTA] & 0xFF);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static inline uint8 ColumnTable::getStartRight()
{
	return (uint8)(_vipRegisters[__CTA] >> 8);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool ColumnTable::buildTables()
{
	// Column table pointer at the start of each eye's display
	ColumnTable::buildDisplay(_lengthsLeft, false, ColumnTable::getStartLeft());
	ColumnTable::buildDisplay(_lengthsRight, true, ColumnTable::getStartRight());

	_tablesBuilt = true;

	return _columnTableEffectSpec->frames <= _age;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void ColumnTable::buildDisplay(uint8* lengths, bool rightEye, uint8 start)
{
	int32 minimumLength = _columnTableCalibration->minimumLength + 1;
	int32 elapsed = 0;

	/// How much of the curves applies to this frame, in 1/256 of a pixel
	int32 shiftAmount = 0;
	int32 depthAmount = 0;

	// Past the last frame both amounts stay at zero, and zero is the plain table
	if(_age < _columnTableEffectSpec->frames)
	{
		shiftAmount = (_columnTableEffectSpec->shiftCurve[_age] * _columnTableEffectSpec->strength) >> __COLUMN_TABLE_STRENGTH_SHIFT;
		depthAmount = (_columnTableEffectSpec->depthCurve[_age] * _columnTableEffectSpec->strength) >> __COLUMN_TABLE_STRENGTH_SHIFT;
	}

	// Walk the reference table from its first group
	_cursorGroup = 0;
	_cursorTime = 0;

	// The left edge cannot move, so everything is measured from where it is
	int32 origin = ColumnTable::getPosition(0, rightEye, shiftAmount, depthAmount);

	for(int32 group = 0; group < __COLUMN_TABLE_GROUPS; group++)
	{
		int32 nextPosition = ColumnTable::getPosition(group + 1, rightEye, shiftAmount, depthAmount);
		int32 target = ColumnTable::timeAt(start, nextPosition - origin);

		// Time this group needs to end where the target says, as a length rounded to the
		// nearest, so that the error never adds up across the screen
		int32 length = ((target - elapsed) + (1 << (__COLUMN_TABLE_GROUP_TIME_SHIFT - 1))) 
			>> __COLUMN_TABLE_GROUP_TIME_SHIFT;

		if(length < minimumLength)
		{
			length = minimumLength;
		}
		else if(__COLUMN_TABLE_MAXIMUM_LENGTH < length)
		{
			length = __COLUMN_TABLE_MAXIMUM_LENGTH;
		}

		elapsed += length << __COLUMN_TABLE_GROUP_TIME_SHIFT;

		lengths[group] = (uint8)(length - 1);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static int32 ColumnTable::timeAt(uint8 start, int32 position)
{
	if(0 >= position)
	{
		return 0;
	}

	int32 group = position >> __COLUMN_TABLE_FRACTION_BITS;
	int32 fraction = position & (__COLUMN_TABLE_GROUP_UNITS - 1);

	if(__COLUMN_TABLE_REACH <= group)
	{
		group = __COLUMN_TABLE_REACH;
		fraction = 0;
	}

	// Group k of an eye uses the entry start - k. Positions only move a few groups from
	// one lookup to the next, so walking from the last one is cheap
	while(_cursorGroup < group)
	{
		_cursorTime += __COLUMN_TABLE_COLUMNS_PER_GROUP
			* (ColumnTable::getReferenceLength((uint8)(start - _cursorGroup)) + 1);
		_cursorGroup++;
	}

	while(group < _cursorGroup)
	{
		_cursorGroup--;
		_cursorTime -= __COLUMN_TABLE_COLUMNS_PER_GROUP
			* (ColumnTable::getReferenceLength((uint8)(start - _cursorGroup)) + 1);
	}

	// The reference table is calibrated so that columns are evenly spaced, so a position
	// a fraction into a group is reached that fraction into the group's time
	return (_cursorTime << __COLUMN_TABLE_FRACTION_BITS)
		+ __COLUMN_TABLE_COLUMNS_PER_GROUP
			* (ColumnTable::getReferenceLength((uint8)(start - group)) + 1)
			* fraction;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool ColumnTable::commit()
{
	// A frame that started after the left eye's computation has nothing to write
	if(!_tablesBuilt)
	{
		return false;
	}

	// The repeats are read now, so a brightness effect that changed them meanwhile is kept
	const BrightnessRepeatSpec* brightnessRepeatSpec = DisplayUnit::getColorConfig().colorConfig.brightnessRepeat;

	if(NULL == brightnessRepeatSpec)
	{
		brightnessRepeatSpec = (BrightnessRepeatSpec*)&DefaultBrightnessRepeatSpec;
	}

	uint8 startLeft = ColumnTable::getStartLeft();
	uint8 startRight = ColumnTable::getStartRight();

	for(int32 group = 0; group < __COLUMN_TABLE_GROUPS; group++)
	{
		uint16 repeat = (uint16)
		(
			(brightnessRepeatSpec->mirror && (group > ((__BRIGHTNESS_REPEAT_ENTRIES >> 1) - 1)))
				? brightnessRepeatSpec->brightnessRepeat[__BRIGHTNESS_REPEAT_ENTRIES - 1 - group]
				: brightnessRepeatSpec->brightnessRepeat[group]
		);

		_columnTableBaseAddressLeft[(uint8)(startLeft - group)] = (uint16)(((repeat & 0xFF) << 8) | _lengthsLeft[group]);
		_columnTableBaseAddressRight[(uint8)(startRight - group)] = (uint16)(((repeat & 0xFF) << 8) | _lengthsRight[group]);
	}

	_tablesBuilt = false;

	// The frame after the last one of the curves is built flat, so the tables end up
	// as the normal ones before the effect lets go
	if(_columnTableEffectSpec->frames <= _age)
	{
		return true;
	}

	_age++;

	return false;
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
