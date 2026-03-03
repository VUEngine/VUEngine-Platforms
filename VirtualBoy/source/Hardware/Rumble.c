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

/// Determines if the commands are broadcasted asynchronously,
/// defaults to true
static bool _async										__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// Determines if the broadcast of new effects should wait or not for a previous queue effect being
/// completedly broadcasted
static bool _overridePreviousEffect						__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// Index of the command in the queue to broadcast next
static uint8 _rumbleCommandIndex						__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = true;

/// Rumble effect spec being broadcasted
static const RumbleEffectSpec* _rumbleEffectSpec		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = NULL;

/// Cached rumble effect to prevent broadcasting again previous send commands
static RumbleEffectSpec _cachedRumbleEffect				__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::startEffect(const RumbleEffectSpec* rumbleEffect)
{
	_rumbleCommandIndex = 0;

	if(!_overridePreviousEffect && 0 != _rumbleCommandIndex)
	{
		return;
	}

	if(NULL == rumbleEffect)
	{
		return;
	}

	if(_rumbleEffectSpec == rumbleEffect)
	{
		if(rumbleEffect->stop)
		{
			Rumble::stop();
		}

		Rumble::play();
		Rumble::execute();
		return;
	}

	_rumbleEffectSpec = rumbleEffect;

	if(rumbleEffect->stop)
	{
		Rumble::stop();
	}

	Rumble::setFrequency(rumbleEffect->frequency);
	Rumble::setSustainPositive(rumbleEffect->sustainPositive);
	Rumble::setSustainNegative(rumbleEffect->sustainNegative);
	Rumble::setOverdrive(rumbleEffect->overdrive);
	Rumble::setBreak(rumbleEffect->breaking);
	Rumble::setEffect(rumbleEffect->effect);
	Rumble::execute();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::stopEffect(const RumbleEffectSpec* rumbleEffect)
{
	if(NULL == rumbleEffect || _rumbleEffectSpec == rumbleEffect)
	{
		Rumble::stop();
		Rumble::execute();
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setAsync(bool async)
{
	_async = async;
	Rumble::stopAllEffects();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setOverridePreviousEffect(bool overridePreviousEffect)
{
	_overridePreviousEffect = overridePreviousEffect;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::reset()
{
	Rumble::getInstance();
	
	_async = true;
	_overridePreviousEffect = true;
	_rumbleEffectSpec = NULL;
	_rumbleCommandIndex = 0;
	_cachedRumbleEffect.frequency = 0;
	_cachedRumbleEffect.sustainPositive = 0;
	_cachedRumbleEffect.sustainNegative = 0;
	_cachedRumbleEffect.overdrive = 0;
	_cachedRumbleEffect.breaking = 0;

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

static void Rumble::sendCode(uint8 code __attribute__((unused)))
{
	_rumbleCommands[_rumbleCommandIndex++] = code;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::execute()
{
// Rumble only is called in release mode since emulators that don't implement communications, 
// lock when trying to broadcast message throught the EXT port
#ifdef __EMU_ONLY
#if __EMU_ONLY
	return;
#endif
#endif

#ifdef __RELEASE
	if(_async)
	{
		if(_overridePreviousEffect)
		{
			Communications::broadcastDataAsync((uint8*)_rumbleCommands, _rumbleCommandIndex, NULL);
		}
		else
		{
			Communications::broadcastDataAsync
			(
				(uint8*)_rumbleCommands, _rumbleCommandIndex, ListenerObject::safeCast(Rumble::getInstance())
			);
		}
	}
	else
	{
		Communications::broadcastData((uint8*)_rumbleCommands, _rumbleCommandIndex);
		_rumbleCommandIndex = 0;
	}
#endif
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::toggleAsync()
{
	_async = !_async;
	Rumble::stopAllEffects();
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
	
	for(i=0; effectChain[i] != __RUMBLE_EFFECT_CHAIN_END && i < __RUMBLE_MAX_EFFECTS_IN_CHAIN; i++)
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
	
	if(value <= __RUMBLE_FREQ_400HZ)
	{
		value += __RUMBLE_CMD_FREQ_50HZ;
	}

	if(value >= __RUMBLE_CMD_FREQ_50HZ && value <= __RUMBLE_CMD_FREQ_400HZ)
	{
		Rumble::sendCode(value);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setOverdrive(uint8 value)
{
	if(_cachedRumbleEffect.overdrive == value)
	{
		return;
	}

	if(__RUMBLE_MAX_OVERDRIVE < value)
	{
		value = __RUMBLE_MAX_OVERDRIVE;
	}

	_cachedRumbleEffect.overdrive = value;

	Rumble::sendCommandWithValue(__RUMBLE_CMD_OVERDRIVE, value);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setSustainPositive(uint8 value)
{
	if(_cachedRumbleEffect.sustainPositive == value)
	{
		return;
	}

	_cachedRumbleEffect.sustainPositive = value;

	Rumble::sendCommandWithValue(__RUMBLE_CMD_SUSTAIN_POS, value);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setSustainNegative(uint8 value)
{
	if(_cachedRumbleEffect.sustainNegative == value)
	{
		return;
	}

	_cachedRumbleEffect.sustainNegative = value;

	Rumble::sendCommandWithValue(__RUMBLE_CMD_SUSTAIN_NEG, value);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::setBreak(uint8 value)
{
	if(_cachedRumbleEffect.breaking == value)
	{
		return;
	}

	_cachedRumbleEffect.breaking = value;

	Rumble::sendCommandWithValue(__RUMBLE_CMD_BREAK, value);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Rumble::play()
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
	Rumble::execute();
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
