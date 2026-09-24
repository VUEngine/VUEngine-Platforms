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

#include <Communications.h>
#ifdef __DEBUG_TOOL
#include <Debug.h>
#endif
#include <DebugConfig.h>
#include <Singleton.h>

#include "Rumble.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Queue of commands to broadcast
static uint8 _rumbleCommands[__RUMBLE_TOTAL_COMMANDS]	__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE;

/// Index of the command in the queue to broadcast next
static uint8 _rumbleCommandIndex						= true;

/// Rumble effect spec being broadcasted
static const RumbleEffectSpec* _rumbleEffectSpec		= NULL;

/// Cached rumble effect to prevent broadcasting again previous send commands
static RumbleEffectSpec _cachedRumbleEffect				__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static bool Rumble::startEffect(const RumbleEffectSpec* rumbleEffect, bool override)
{
	if(NULL == rumbleEffect)
	{
		return false;
	}

	if(override)
	{
		if(0 != _rumbleCommandIndex)
		{
			Communications::cancelBroadcasts();
			Rumble::reset();

			if(!rumbleEffect->stop)
			{
				Rumble::stop();
			}
		}
	}
	else
	{
		if(0 != _rumbleCommandIndex)
		{
			return false;
		}
	}

	if(_rumbleEffectSpec == rumbleEffect)
	{
		if(rumbleEffect->stop)
		{
			Rumble::stop();
		}

		Rumble::restart();
		Rumble::execute(true);
		return true;
	}

	_rumbleEffectSpec = rumbleEffect;

	if(rumbleEffect->stop)
	{
		Rumble::stop();
	}

	Rumble::setFrequency(rumbleEffect->frequency);
	Rumble::setEffect(rumbleEffect->effect);
	Rumble::execute(true);

	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::stopEffect(const RumbleEffectSpec* rumbleEffect)
{
	if(NULL == rumbleEffect || _rumbleEffectSpec == rumbleEffect)
	{
		Rumble::stop();
		Rumble::execute(true);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::reset()
{
	Rumble::getInstance();
	
	_rumbleEffectSpec = NULL;
	_rumbleCommandIndex = 0;
	
	_cachedRumbleEffect.frequency = 0;

	for(int32 i = 0; i < __RUMBLE_TOTAL_COMMANDS; i++)
	{
		_rumbleCommands[i] = 0;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool Rumble::onEvent(ListenerObject eventFirer, uint16 eventCode)
{
	switch(eventCode)
	{
		case kEventCommunicationsTransmissionCompleted:
		{
			_rumbleCommandIndex = 0;

			return false;
		}
	}

	return Base::onEvent(this, eventFirer, eventCode);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::sendCode(uint8 code)
{
	if(_rumbleCommandIndex < __RUMBLE_TOTAL_COMMANDS - 1)
	{
		_rumbleCommands[_rumbleCommandIndex++] = code;
	}
	else
	{
		Rumble::execute(false);

		if(0 == _rumbleCommandIndex)
		{
			_rumbleCommands[_rumbleCommandIndex++] = code;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::execute(bool async)
{
	if(async)
	{
		Communications::broadcastDataAsync((uint8*)_rumbleCommands, _rumbleCommandIndex, ListenerObject::safeCast(Rumble::getInstance()));
	}
	else
	{
		Communications::broadcastData((uint8*)_rumbleCommands, _rumbleCommandIndex);
		_rumbleCommandIndex = 0;	
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::sendCommandWithValue(uint8 command, uint8 value)
{
	Rumble::sendCode(command);
	Rumble::sendCode(value);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setEffect(uint8 effect)
{
	if(effect >= __RUMBLE_CMD_MIN_EFFECT && effect <= __RUMBLE_CMD_MAX_EFFECT)
	{
		Rumble::sendCode(effect);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::storeEffectChain(uint8 chainNumber, uint8* effectChain)
{
	uint8 i = 0;
	
	Rumble::sendCode(__RUMBLE_CMD_WRITE_EFFECT_CHAIN);
	
	Rumble::sendCode(chainNumber);
	
	for(i = 0; effectChain[i] != __RUMBLE_EFFECT_CHAIN_END && i < __RUMBLE_MAX_EFFECTS_IN_CHAIN; i++)
	{
		Rumble::setEffect(effectChain[i]);
	}

	Rumble::sendCode(__RUMBLE_EFFECT_CHAIN_END);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setEffectChain(uint8 effectChain)
{
	uint8 command = effectChain;

	if(command <= __RUMBLE_CHAIN_EFFECT_4)
	{
		command += __RUMBLE_CMD_CHAIN_EFFECT_0;
	}

	if(command >= __RUMBLE_CMD_CHAIN_EFFECT_0 && command <= __RUMBLE_CMD_CHAIN_EFFECT_4)
	{
		Rumble::sendCode(command);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setFrequency(uint8 value)
{
	if(_cachedRumbleEffect.frequency == value)
	{
		return;
	}

	_cachedRumbleEffect.frequency = value;

	if(__RUMBLE_FREQ_160HZ <= (int8)value && __RUMBLE_FREQ_320HZ >= (int8)value)
	{
		value += __RUMBLE_CMD_FREQ_160HZ - __RUMBLE_FREQ_160HZ;
	}
	
	if(__RUMBLE_CMD_FREQ_160HZ <= value && __RUMBLE_CMD_FREQ_320HZ >= value)
	{
		Rumble::sendCode(value);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::restart()
{
	Rumble::sendCode(__RUMBLE_CMD_PLAY);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::stop()
{
	Rumble::sendCode(__RUMBLE_CMD_STOP);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::stopAllEffects()
{
	Rumble::stop();
	Rumble::execute(true);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Rumble::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Rumble::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
