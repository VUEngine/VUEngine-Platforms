/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with  source code.
 */

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <Platform.h>
#include <Printer.h>
#include <Singleton.h>

#include <Keypad.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

extern uint8* const _hardwareRegisters;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 volatile* _readingStatus 		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = NULL;

/// Holds the mathematical sum of all user's presses
static uint32 _accumulatedUserInput			__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = 0;

/// Struct that holds the user's input during the last game frame
static UserInput _userInput					__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE;

/// Flags to select which inputs to register and which to ignore
static UserInput _userInputToRegister		__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE;

/// Flag to enable/disable the capture of user input
static bool _enabled						__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

/// Flag to prevent pressed and released keys from being raised when
/// holding buttons while changing game states
static bool _reseted						__STATIC_SINGLETONS_DATA_SECTION_ATTRIBUTE = false;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::interruptHandler()
{
	Keypad::disableInterrupt();
#ifndef __RELEASE
	Printer::text("KYP interrupt", 48 - 13, 26, NULL);
	Printer::hex((((_hardwareRegisters[__SDHR] << 8)) | _hardwareRegisters[__SDLR]), 48 - 13, 27, 8, NULL);
#endif
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::enable()
{
	_enabled = true;
	_hardwareRegisters[__SCR] = (__S_INTDIS | __S_HW);
	Keypad::flush();
	_reseted = true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::disable()
{	
	_enabled = false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static int32 Keypad::isEnabled()
{	
	return _enabled;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::enableDummyKey()
{	
	_userInput.dummyKey = K_ANY;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::disableDummyKey()
{	
	_userInput.dummyKey = K_NON;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::registerInput(uint16 inputToRegister)
{	
#ifdef __TOOLS
	inputToRegister = __KEY_PRESSED | __KEY_RELEASED | __KEY_HOLD;
#endif
	_userInputToRegister.pressedKey = __KEY_PRESSED & inputToRegister? 0xFFFF : 0;
	_userInputToRegister.releasedKey = __KEY_RELEASED & inputToRegister? 0xFFFF : 0;
	_userInputToRegister.holdKey = __KEY_HOLD & inputToRegister? 0xFFFF : 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static UserInput Keypad::getUserInput()
{
	return _userInput;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 Keypad::getAccumulatedUserInput()
{	
	return _accumulatedUserInput;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifndef __SHIPPING
static void Keypad::printUserInput(int32 x, int32 y)
{	
	int32 xDisplacement = 13;

	PRINT_TEXT("USER INPUT:", x, y++);

	PRINT_TEXT("allKeys:", x, ++y);
	PRINT_HEX(_userInput.allKeys, x + xDisplacement, y);

	PRINT_TEXT("pressedKey:", x, ++y);
	PRINT_HEX(_userInput.pressedKey, x + xDisplacement, y);

	PRINT_TEXT("releasedKey:", x, ++y);
	PRINT_HEX(_userInput.releasedKey, x + xDisplacement, y);

	PRINT_TEXT("holdKey:", x, ++y);
	PRINT_HEX(_userInput.holdKey, x + xDisplacement, y);

	PRINT_TEXT("holdKeyDuration:", x, ++y);
	PRINT_HEX(_userInput.holdKeyDuration, x + xDisplacement, y);

	PRINT_TEXT("powerFlag:", x, ++y);
	PRINT_HEX(_userInput.powerFlag, x + xDisplacement, y);
}
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::reset()
{
	Keypad::getInstance();

	Keypad::flush();

	_readingStatus = (uint32 *)&_hardwareRegisters[__SCR];
	_reseted = true;
	_enabled = false;
	_accumulatedUserInput = 0;
	_userInputToRegister = (UserInput){0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
	_userInput.dummyKey = K_NON;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::readUserInput(bool waitForStableReading)
{
	if(!waitForStableReading)
	{
		if(*_readingStatus & __S_STAT)
		{
			_userInput = (UserInput){K_NON, K_NON, K_NON, K_NON, K_NON, K_NON, K_NON, K_NON};
			return;
		}
	}
	else
	{
		// Wait for keypad to stabilize
		while(*_readingStatus & __S_STAT);
	}

	// Now read the keys
	_userInput.allKeys = (((_hardwareRegisters[__SDHR] << 8)) | _hardwareRegisters[__SDLR]);

	// Enable next reading cycle
	_hardwareRegisters[__SCR] = (__S_INTDIS | __S_HW);

	// Store keys
	if(0 == _userInput.powerFlag)
	{
		if(K_PWR == (_userInput.allKeys & K_PWR))
		{
			Keypad::fireEvent(Keypad::getInstance(), kEventKeypadRaisedPowerFlag);

			_userInput.powerFlag = _userInput.allKeys & K_PWR;
		}
	}

	_userInput.allKeys &= K_ANY;

	if(_reseted)
	{
		_userInput.pressedKey = 0;
		_userInput.releasedKey = 0;
	}
	else
	{
		_userInput.pressedKey = Keypad::getPressedKey() & _userInputToRegister.pressedKey;
		_userInput.releasedKey = Keypad::getReleasedKey() & _userInputToRegister.releasedKey;
	}

	_userInput.holdKey = Keypad::getHoldKey() & _userInputToRegister.holdKey;
	_userInput.previousKey = _userInput.allKeys;
	_userInput.holdKeyDuration = (_userInput.holdKey && _userInput.holdKey == _userInput.previousKey)
		? _userInput.holdKeyDuration + 1
		: 0;

	_accumulatedUserInput += _userInput.allKeys;

	_reseted = false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static void Keypad::enableInterrupt()
{
	Keypad::flush();

	_hardwareRegisters[__SCR] = 0;
	_hardwareRegisters[__SCR] &= ~(__S_HWDIS | __S_INTDIS);
	_hardwareRegisters[__SCR] |= __S_HW;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::disableInterrupt()
{
	_hardwareRegisters[__SCR] |= __S_INTDIS;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Keypad::flush()
{
	_userInput = (UserInput){K_NON, K_NON, K_NON, K_NON, K_NON, K_NON, K_NON, _userInput.dummyKey};
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Keypad::getPressedKey()
{
	return _userInput.allKeys & ~_userInput.previousKey;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Keypad::getReleasedKey()
{
	return ~_userInput.allKeys & _userInput.previousKey;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Keypad::getHoldKey()
{
	return _userInput.allKeys & _userInput.previousKey;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Keypad::getPreviousKey()
{
	return _userInput.previousKey;
}


//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Keypad::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Keypad::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
