/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

/* This is based on Thunderstrucks' ColumnTable Pak library */

#ifndef __COLUMN_TABLE_H_
#define __COLUMN_TABLE_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <ListenerObject.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// A ColumnTable effect spec
/// @memberof ColumnTable
typedef struct ColumnTableEffectSpec
{
	/// How intense the effect is, as a fixed-point factor where 256 plays the spec exactly
	/// as authored, 128 plays it at half the amount and 512 at double
	int32 strength;

	/// Horizontal displacement over time, one entry per frame. It moves both eyes together
	/// (the picture slides sideways). Values are in 1/256 of a pixel and are multiplied by
	/// the shift profile to get each group's offset
	const int16* shiftCurve;

	/// Depth change over time, one entry per frame. It moves the eyes in opposite directions
	/// (the picture comes nearer or goes farther). Values are in 1/256 of a pixel of disparity
	/// and are multiplied by the depth profile to get each group's disparity
	const int16* depthCurve;

	/// Number of frames the effect lasts, which is the length of both curves.
	/// The effect stops once this many frames have been played
	uint16 frames;

	/// How much of the shift curve each group edge receives, from one end of the screen to the
	/// other (one entry per group edge, so one more than the number of groups). A fixed-point
	/// fraction where 16384 means the full curve value, negative values move the opposite
	/// way and 0 leaves that edge untouched
	const int16* shiftProfile;

	/// How much of the depth curve each group edge receives, from one end of the screen to the
	/// other (one entry per group edge, so one more than the number of groups). A fixed-point
	/// fraction where 16384 means the full curve value, negative values move the opposite
	/// way and 0 leaves that edge at the screen plane
	const int16* depthProfile;

} ColumnTableEffectSpec;

/// A ColumnTableEffect spec that is stored in ROM
/// @memberof ColumnTable
typedef const ColumnTableEffectSpec ColumnTableEffectROMSpec;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATION
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Class ColumnTable
///
/// Inherits from Object
///
/// Manages ColumnTable effects.
singleton class ColumnTable : ListenerObject
{
	/// Start playing a spec from its first frame, replacing any effect already in progress
	/// @param columnTableEffectSpec: Curves and profiles to play
	static void startEffect(const ColumnTableEffectSpec* columnTableEffectSpec);

	/// Stop the effect right away and restore the normal column table
	/// @param rumbleEffectSpec: Specification of the rumble effect to stop; if NULL,
	/// any playing effect is stoped
	static void stopEffect(const ColumnTableEffectSpec* columnTableEffectSpec);

	/// Process an event that the instance is listen for.
	/// @param eventFirer: ListenerObject that signals the event
	/// @param eventCode: Code of the firing event
	/// @return False if the listener has to be removed; true to keep it
	override bool onEvent(ListenerObject eventFirer, uint16 eventCode);
};

#endif
