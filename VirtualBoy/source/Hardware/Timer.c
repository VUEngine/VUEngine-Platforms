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
#include <Hardware.h>
#include <ListenerObject.h>
#include <Platform.h>
#include <Printer.h>
#include <Profiler.h>
#include <Singleton.h>
#include <VUEngine.h>

#include <Timer.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' MACROS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define __MAXIMUM_TIME_PER_INTERRUPT_US 			(1.3f * 1000)
#define __MINIMUM_TIME_PER_INTERRUPT_MS				1
#define __MAXIMUM_TIME_PER_INTERRUPT_MS 			49

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

extern uint8* const _hardwareRegisters;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Elapsed milliseconds since the last call to reset
static uint32 _elapsedMilliseconds = 0;

/// Elapsed microseconds
static uint32 _elapsedMicroseconds = 0;

/// Elapsed milliseconds since the start of the program
static uint32 _totalElapsedMilliseconds = 0;

/// Timer's resolution
static uint16 _resolution = 0;

/// Interrupts during the last secoond
static uint16 _interruptsPerSecond = 0;

/// Interrupts during the last game frame
static uint16 _interruptsPerGameFrame = 0;

/// Elapsed microseconds per interrupt
static uint32 _elapsedMicrosecondsPerInterrupt = 0;

/// Target elapsed time per interrupt
static uint16 _targetTimePerInterrupt = 0;

/// Units of the target time per interrupt
static uint16 _targetTimePerInterrupttUnits = 0;

/// Last written value to the TCR registry
static uint8 _tcrValue = 0;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::interruptHandler()
{
	//disable
	Timer::disableInterrupt();
	Timer::clearStat();

#ifdef __ENABLE_PROFILER
	Profiler::lap(kProfilerLapTypeStartInterrupt, NULL);
#endif

	_interruptsPerSecond++;

	_interruptsPerGameFrame++;

	_elapsedMicroseconds += _elapsedMicrosecondsPerInterrupt;

	if(__MICROSECONDS_PER_MILLISECOND < _elapsedMicroseconds)
	{
		uint32 elapsedMilliseconds = _elapsedMicroseconds / __MICROSECONDS_PER_MILLISECOND;

		_elapsedMicroseconds = _elapsedMicroseconds % __MICROSECONDS_PER_MILLISECOND;

		_elapsedMilliseconds += elapsedMilliseconds;

		_totalElapsedMilliseconds += elapsedMilliseconds;
	}

	Timer::fireEvent(Timer::getInstance(), kEventTimerInterrupt);

	// enable
	Timer::enableInterrupt();

#ifdef __ENABLE_PROFILER
	Profiler::lap(kProfilerLapTypeTimerInterruptProcess, PROCESS_NAME_SOUND_PLAY);
#endif
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::configure(TimerConfig timerConfig)
{
	Timer::setResolution(timerConfig.resolution);
	Timer::setTargetTimePerInterruptUnits(timerConfig.targetTimePerInterrupttUnits);
	Timer::setTargetTimePerInterrupt(timerConfig.targetTimePerInterrupt);
	Timer::applySettings(true);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::applySettings(bool enable)
{
	Timer::disable();
	Timer::initialize();

	if(enable)
	{
		Timer::enable();
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::enable()
{
	_tcrValue |= __TIMER_ENB | __TIMER_Z_INT;

	_hardwareRegisters[__TCR] = _tcrValue;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::disable()
{
	_tcrValue &= ~(__TIMER_ENB | __TIMER_Z_INT);

	_hardwareRegisters[__TCR] = _tcrValue;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::resetTimerCounter()
{
	uint16 timerCounter = Timer::computeTimerCounter();

	switch(_resolution)
	{
		case __TIMER_20US:
		{
			break;
		}

		case __TIMER_100US:
		{
			// Compensate for the difference in speed between 20US and 100US timer resolution
			timerCounter += __TIMER_COUNTER_DELTA;
			break;
		}
	}

	_hardwareRegisters[__TLR] = (timerCounter & 0xFF);
	_hardwareRegisters[__THR] = (timerCounter >> 8);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::setResolution(uint16 resolution)
{
	switch(resolution)
	{
		case __TIMER_20US:
		{
			_resolution = resolution;
			break;
		}

		case __TIMER_100US:
		{
			_resolution = resolution;
			break;
		}

		default:
		{
			NM_ASSERT(false, "Timer::setResolution: wrong timer resolution");

			_resolution =  __TIMER_20US;
			break;
		}
	}

	uint32 targetTimePerInterrupt = _targetTimePerInterrupt;

	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
			uint32 residue = targetTimePerInterrupt % Timer::getResolutionInUS();

			if(targetTimePerInterrupt > residue)
			{
				targetTimePerInterrupt -= residue;
			}

			break;
		}

		case kMS:
		{
			break;
		}

		default:
		{
			ASSERT(false, "Timer::setResolution: wrong timer resolution scale");
			break;
		}
	}

	Timer::setTargetTimePerInterrupt(targetTimePerInterrupt);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getResolution()
{
	return _resolution;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getResolutionInUS()
{
	switch(_resolution)
	{
		case __TIMER_20US:
		{
			return 20;
			break;
		}

		case __TIMER_100US:
		{
			return 100;
			break;
		}

		default:
		{
			ASSERT(false, "Timer::getResolutionInUS: wrong timer resolution");
			break;
		}
	}

	return 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::setTargetTimePerInterrupt(uint16 targetTimePerInterrupt)
{
	int16 minimumTimePerInterrupt = 0;
	int16 maximumTimePerInterrupt = 1000;

	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
#ifdef __DEBUG
			targetTimePerInterrupt = 1000 > targetTimePerInterrupt ? 1000 : targetTimePerInterrupt;
#endif

			minimumTimePerInterrupt = 
				Timer::getResolutionInUS() + Timer::getResolutionInUS() * __TIMER_COUNTER_DELTA;
			maximumTimePerInterrupt = __MAXIMUM_TIME_PER_INTERRUPT_US;
			break;
		}

		case kMS:
		{
			minimumTimePerInterrupt = __MINIMUM_TIME_PER_INTERRUPT_MS;
			maximumTimePerInterrupt = __MAXIMUM_TIME_PER_INTERRUPT_MS;
			break;
		}

		default:
		{
			ASSERT(false, "Timer::setTargetTimePerInterrupt: wrong resolution scale");
			break;
		}
	}

	if((int16)targetTimePerInterrupt < minimumTimePerInterrupt)
	{
		targetTimePerInterrupt = minimumTimePerInterrupt;
	}
	else if((int16)targetTimePerInterrupt > maximumTimePerInterrupt)
	{
		targetTimePerInterrupt = maximumTimePerInterrupt;
	}

	_targetTimePerInterrupt = targetTimePerInterrupt;
	_elapsedMicrosecondsPerInterrupt = Timer::getTargetTimePerInterruptInUS();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getTargetTimePerInterrupt()
{
	return _targetTimePerInterrupt;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static float Timer::getTargetTimePerInterruptInMS()
{
	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
			return _targetTimePerInterrupt / (float)__MICROSECONDS_PER_MILLISECOND;
			break;
		}

		case kMS:
		{
			return _targetTimePerInterrupt;
			break;
		}

		default:
		{
			ASSERT(false, "Timer::getTargetTimePerInterruptInMS: wrong timer resolution scale");
			break;
		}
	}

	return 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 Timer::getTargetTimePerInterruptInUS()
{
	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
			return _targetTimePerInterrupt;
			break;
		}
		
		case kMS:
		{
			return _targetTimePerInterrupt * __MICROSECONDS_PER_MILLISECOND;
			break;
		}

		default:
		{
			ASSERT(false, "Timer::getTargetTimePerInterruptInUS: wrong timer resolution scale");
			break;
		}
	}

	return 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::setTargetTimePerInterruptUnits(uint16 targetTimePerInterrupttUnits)
{
	switch(targetTimePerInterrupttUnits)
	{
		case kUS:
		case kMS:
		{
			_targetTimePerInterrupttUnits = targetTimePerInterrupttUnits;
			break;
		}

		default:
		{
			ASSERT(false, "Timer::setTargetTimePerInterruptUnits: wrong resolution scale");
			break;
		}
	}

	Timer::setResolution(_resolution);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getTargetTimePerInterruptUnits()
{
	return _targetTimePerInterrupttUnits;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getTimerCounter()
{
	return Timer::computeTimerCounter();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getCurrentTimerCounter()
{
	return (_hardwareRegisters[__THR] << 8 ) | _hardwareRegisters[__TLR];
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::getMinimumTimePerInterruptStep()
{
	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
			return Timer::getResolutionInUS();
		}

		case kMS:
		{
			return 1;
		}
	}

	return 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 Timer::getElapsedMilliseconds()
{
	return _elapsedMilliseconds;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint32 Timer::getTotalElapsedMilliseconds()
{
	return _totalElapsedMilliseconds;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::wait(uint32 milliseconds)
{
	Hardware::enableInterrupts();
	
	// Declare as volatile to prevent the compiler to optimize currentMilliseconds away
	// Making the last assignment invalid
	volatile uint32 currentMilliseconds = _totalElapsedMilliseconds;
	uint32 waitStartTime = _totalElapsedMilliseconds;
	volatile uint32 *totalElapsedMilliseconds = (uint32*)&_totalElapsedMilliseconds;

	while ((*totalElapsedMilliseconds - waitStartTime) < milliseconds);

	_elapsedMilliseconds = currentMilliseconds;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::repeatMethodCall(uint32 callTimes, uint32 duration, ListenerObject object, void (*method)(ListenerObject, uint32))
{
	if(!isDeleted(object) && method)
	{
		// Declare as volatile to prevent the compiler to optimize currentMilliseconds away
		// Making the last assignment invalid
		volatile uint32 currentMilliseconds = _elapsedMilliseconds;

		uint32 i = 0;

		for(; i < callTimes; i++)
		{
			Timer::wait(duration / callTimes);

			if(isDeleted(object))
			{
				return;
			}

			method(object, i);
		}

		_elapsedMilliseconds = currentMilliseconds;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::print(int32 x, int32 y)
{
	Printer::text("TIMER CONFIG", x, y++, NULL);
	y++;

	switch(_resolution)
	{
		case __TIMER_20US:
		{
			Printer::text("Resolution    20 US ", x, y++, NULL);
			break;
		}

		case __TIMER_100US:
		{
			Printer::text("Resolution    100 US ", x, y++, NULL);
			break;
		}

		default:
		{
			Printer::text("Resolution    ?      ", x, y++, NULL);
			break;
		}
	}

	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
			Printer::text("US/interrupt        ", x, y, NULL);
			break;
		}

		case kMS:
		{
			Printer::text("MS/interrupt        ", x, y, NULL);
			break;
		}

		default:
		{
			Printer::text(" ?/interrupt        ", x, y, NULL);
			break;
		}
	}

	Printer::int32(_targetTimePerInterrupt, x + 14, y++, NULL);

	Printer::text("Timer counter        ", x, y, NULL);
	Printer::int32(Timer::getTimerCounter(), x + 14, y++, NULL);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::printStats(int x, int y)
{
	PRINT_TEXT("TIMER STATUS", x, y++);
	PRINT_TEXT("Inter./sec.:          ", x, y);
	PRINT_INT(_interruptsPerSecond, x + 17, y++);
	PRINT_TEXT("Inter./frm:           ", x, y);
	PRINT_INT(_interruptsPerSecond / __TARGET_FPS, x + 17, y++);
	PRINT_TEXT("Aver. us/inter.:      ", x, y);
	PRINT_INT(__MICROSECONDS_PER_SECOND / _interruptsPerSecond, x + 17, y++);
	PRINT_TEXT("Real us/inter.:       ", x, y);
	PRINT_INT(_elapsedMicrosecondsPerInterrupt, x + 17, y++);

	_interruptsPerSecond = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::reset()
{
	Timer::getInstance();
	
	_tcrValue = 0;
	_elapsedMilliseconds = 0;
	_elapsedMicroseconds = 0;
	_resolution = __TIMER_100US;
	_targetTimePerInterrupt = 1;
	_targetTimePerInterrupttUnits = kMS;
	_interruptsPerGameFrame = 0;
	_totalElapsedMilliseconds = 0;
	_elapsedMicrosecondsPerInterrupt = Timer::getTargetTimePerInterruptInUS();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::frameStarted(uint32 elapsedMicroseconds)
{
	_elapsedMilliseconds = 0;
	_elapsedMicroseconds = 0;

	if(0 >= _interruptsPerGameFrame)
	{
		_elapsedMicrosecondsPerInterrupt = Timer::getTargetTimePerInterruptInUS();
	}
	else
	{
		_elapsedMicrosecondsPerInterrupt = elapsedMicroseconds / _interruptsPerGameFrame;
	}

	_interruptsPerGameFrame = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static fix7_9_ext Timer::computeTimerResolutionFactor(uint32 targetTimerResolutionUS, uint32 targetUSPerTick)
{
	targetTimerResolutionUS = 
		0 != targetTimerResolutionUS
		? 
		targetTimerResolutionUS : 1000;

	uint32 timerCounter = Timer::getTimerCounter() + __TIMER_COUNTER_DELTA;
	uint32 timerUsPerInterrupt = timerCounter * targetUSPerTick;
	
	targetTimerResolutionUS = 0 == targetTimerResolutionUS? 1000: targetTimerResolutionUS;
	
	uint32 targetUsPerInterrupt = (__TIME_US(targetTimerResolutionUS) + __TIMER_COUNTER_DELTA) * targetUSPerTick;

	NM_ASSERT(0 < targetUsPerInterrupt, "Timer::computeTimerResolutionFactor: zero targetUsPerInterrupt");

	return __FIX19_13_TO_FIX7_9_EXT(__FIX19_13_DIV(__I_TO_FIX19_13(timerUsPerInterrupt), __I_TO_FIX19_13(targetUsPerInterrupt)));
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::initialize()
{
	Timer::disableInterrupt();
	Timer::setTimerResolution();
	Timer::clearStat();
	Timer::resetTimerCounter();
	Timer::enable();
	Timer::enableInterrupt();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static uint16 Timer::computeTimerCounter()
{
	int16 timerCounter = 0;

	switch(_targetTimePerInterrupttUnits)
	{
		case kUS:
		{
			timerCounter = __TIME_US(_targetTimePerInterrupt);
			break;
		}
		
		case kMS:
		{
			timerCounter = __TIME_MS(_targetTimePerInterrupt);
			break;
		}

		default:
		{
			NM_ASSERT(false, "Timer::setTargetTimePerInterruptUnits: wrong resolution scale");
			break;
		}
	}

	return (uint16)(0 >= timerCounter ? 1 : timerCounter);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::enableInterrupt()
{
	_tcrValue |= __TIMER_Z_INT;

	_hardwareRegisters[__TCR] = _tcrValue;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::disableInterrupt()
{
	_tcrValue &= ~__TIMER_Z_INT;

	_hardwareRegisters[__TCR] = _tcrValue;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::setTimerResolution()
{
	_tcrValue = (_tcrValue & 0x0F) | _resolution;
	_hardwareRegisters[__TCR] = _tcrValue;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void Timer::clearStat()
{
	_hardwareRegisters[__TCR] = (_tcrValue | __TIMER_Z_STAT_ZCLR);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Timer::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void Timer::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
