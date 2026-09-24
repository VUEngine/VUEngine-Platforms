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
	/// @param override: If true, any playing effect will be overrode
	/// @return True if the effect was started
	static bool startEffect(const RumbleEffectSpec* rumbleEffectSpec, bool override);

	/// Stop a rumble effect configured with the provided spec.
	/// @param rumbleEffectSpec: Specification of the rumble effect to stop; if NULL,
	/// any playing effect is stoped
	static void stopEffect(const RumbleEffectSpec* rumbleEffectSpec);

	/// Reset the manager's state.
	static void reset();

	/// Process an event that the instance is listen for.
	/// @param eventFirer: ListenerObject that signals the event
	/// @param eventCode: Code of the firing event
	/// @return False if the listener has to be removed; true to keep it
	override bool onEvent(ListenerObject eventFirer, uint16 eventCode);
}

#endif
