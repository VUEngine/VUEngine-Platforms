/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

/* This is based on Thunderstrucks' Rumble Pak library */

#ifndef __RUMBLE_MANAGER_H_
#define __RUMBLE_MANAGER_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <ListenerObject.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// A rumble effect spec
/// @memberof Rumble
typedef struct RumbleEffectSpec
{
	/// Effect number
	uint8 effect;

	/// Frequency
	uint8 frequency;

	/// Positive Sustain
	uint8 sustainPositive;

	/// Negative Sustain
	uint8 sustainNegative;

	/// Overdrive
	uint8 overdrive;

	/// Break
	uint8 breaking;

	/// Stop before starting
	bool stop;

} RumbleEffectSpec;

/// A RumbleEffect spec that is stored in ROM
/// @memberof Rumble
typedef const RumbleEffectSpec RumbleEffectROMSpec;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATION
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Class Rumble
///
/// Inherits from Object
///
/// Manages rumble effects.
singleton class Rumble : ListenerObject
{
	/// @publicsection

	/// Start a rumble effect configured with the provided spec.
	/// @param rumbleEffectSpec: Specification of the rumble effect to play
	static void startEffect(const RumbleEffectSpec* rumbleEffectSpec);

	/// Stop a rumble effect configured with the provided spec.
	/// @param rumbleEffectSpec: Specification of the rumble effect to stop; if NULL,
	/// any playing effect is stoped
	static void stopEffect(const RumbleEffectSpec* rumbleEffectSpec);

	/// Set the async flag.
	/// @param async: If true, rumble commands are broadcasted asynchronously
	static void setAsync(bool async);

	/// Set the flag to broadcast new effects regardless of if there is a previous queue effect pending
	/// broadcasted
	/// @param overridePreviousEffect: If true, new effects are broadcasted regardless of if there is a
	/// queued effect pending broadcasting
	static void setOverridePreviousEffect(bool overridePreviousEffect);

	/// Reset the manager's state.
	static void reset();

	/// Process an event that the instance is listen for.
	/// @param eventFirer: ListenerObject that signals the event
	/// @param eventCode: Code of the firing event
	/// @return False if the listener has to be removed; true to keep it
	override bool onEvent(ListenerObject eventFirer, uint16 eventCode);
}

#endif
