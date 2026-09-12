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

#include <Singleton.h>
#include <VirtualList.h>

#include <SoundUnit.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

friend class VirtualNode;
friend class VirtualList;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' MACROS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef __TILE_DARK_RED_BOX
#define __TILE_DARK_RED_BOX						'\x0E'
#undef __TILE_BRIGHT_RED_BOX
#define __TILE_BRIGHT_RED_BOX					'\x10'

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

typedef struct SoundSourceEntry
{
	int16 soundSourceIndex;
	Waveform* waveform;

} SoundSourceEntry;


//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' ATTRIBUTES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Pointer to the hardware's sound registers
static SoundSource* const _soundSources = (SoundSource*)__SOUND_SOURCES_ADRESS;

/// List of queued sound source configurations
static VirtualList _queuedSoundSourceConfigurationRequests = NULL;

/// Mapping of VSU sound source configurations
static SoundSourceConfiguration _soundSourceConfigurations[__TOTAL_SOUND_SOURCES];

/// Mapping of waveworms
static Waveform _waveforms[__TOTAL_WAVEFORMS];

/// Elapsed ticks
static uint32 _ticks = 0;

/// If false and if there are no sound sources availables at the time of request,
/// the petition is ignored
static bool _allowQueueingSoundRequests = false;

/// Flag to skip sound source releasing if not necessary
static bool _haveUsedSoundSources = false;

/// Flag to skip pending sound source dispatching if not necessary
static bool _haveQueuedRequests = false;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::applySoundSourceConfiguration(const SoundSourceConfigurationRequest* soundSourceConfigurationRequest)
{
	SoundSourceEntry soundSourceEntry = 
		SoundUnit::findSoundSource
		(
			soundSourceConfigurationRequest->requesterId, soundSourceConfigurationRequest->type, 
			soundSourceConfigurationRequest->priority, soundSourceConfigurationRequest->SxRAM
		);

	if(0 > soundSourceEntry.soundSourceIndex)
	{
		if(_allowQueueingSoundRequests && !soundSourceConfigurationRequest->skip)
		{
			SoundUnit::registerQueuedSoundSourceConfigurationRequest(soundSourceConfigurationRequest);
		}
	}
	else
	{
		SoundUnit::configureSoundSource(soundSourceConfigurationRequest, soundSourceEntry);
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::stopSoundSourcesUsedBy(uint32 requesterId)
{
	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		if(requesterId == _soundSourceConfigurations[i].requesterId)
		{
			_soundSourceConfigurations[i].timeout = 0;
			_soundSourceConfigurations[i].waveform = NULL;
			_soundSourceConfigurations[i].SxINT = 0;
			_soundSourceConfigurations[i].soundSource->SxINT = 0;
		}
	}	

	for(VirtualNode node = _queuedSoundSourceConfigurationRequests->head, nextNode = NULL; NULL != node; node = nextNode)
	{
		nextNode = node->next;

		SoundSourceConfigurationRequest* queuedSoundSourceConfigurationRequest = (SoundSourceConfigurationRequest*)node->data;

		if(requesterId == queuedSoundSourceConfigurationRequest->requesterId)
		{
			VirtualList::removeNode(_queuedSoundSourceConfigurationRequests, node);
			delete queuedSoundSourceConfigurationRequest;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static const SoundSourceConfiguration* SoundUnit::getSoundSourceConfigurations()
{
	return _soundSourceConfigurations;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifndef __SHIPPING
static void SoundUnit::print(int32 x, int32 y)
{
	int32 xDisplacement = 15;
	int32 yDisplacement = y;

	int32 i = 0;

	// Reset all sounds and channels
	for(i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		int32 y = yDisplacement;

		SoundUnit::printSoundSourceConfiguration(&_soundSourceConfigurations[i], x, y);
		
		x += xDisplacement;
		if(x > 47 - xDisplacement)
		{
			x = 1;
			yDisplacement += 15;
		}
	}
}
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifndef __SHIPPING
static void SoundUnit::printWaveFormStatus(int32 x, int32 y)
{
	for(uint32 i = 0; i < __TOTAL_WAVEFORMS; i++)
	{
		PRINT_TEXT("           ", x, y + _waveforms[i].index);
		PRINT_INT(_waveforms[i].index, x, y + _waveforms[i].index);
		PRINT_INT(_waveforms[i].usageCount, x + 4, y + _waveforms[i].index);
		PRINT_HEX((uint32)_waveforms[i].data, x + 8, y + _waveforms[i].index);
	}
}
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifndef __SHIPPING
static void SoundUnit::printSoundSourceConfiguration
(
	const SoundSourceConfiguration* soundSourceConfiguration, int16 x, int y
)
{
	if(NULL == soundSourceConfiguration)
	{
		return;
	}

	PRINT_TEXT("TIMEO:         ", x, ++y);
	PRINT_INT(soundSourceConfiguration->timeout, x + 7, y);

	PRINT_TEXT("SXINT:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxINT, x + 7, y, 2);

	PRINT_TEXT("SXLRV:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxLRV, x + 7, y, 2);

	PRINT_TEXT("SXFQL:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxFQL, x + 7, y, 2);

	PRINT_TEXT("SXFQH:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxFQH, x + 7, y, 2);

	PRINT_TEXT("SXEV0:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxEV0, x + 7, y, 2);

	PRINT_TEXT("SXEV1:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxEV1, x + 7, y, 2);

	PRINT_TEXT("SXRAM:         ", x, ++y);
	PRINT_HEX_EXT(0x0000FFFF & (uint32)soundSourceConfiguration->SxRAM, x + 7, y, 2);

	PRINT_TEXT("SXSWP:         ", x, ++y);
	PRINT_HEX_EXT(soundSourceConfiguration->SxSWP, x + 7, y, 2);
}
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifndef __SHIPPING
static void SoundUnit::printChannels(int32 x, int32 y)
{
	uint16 totalVolume = 0;

	for(uint16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		uint16 volume = _soundSourceConfigurations[i].SxLRV;

		totalVolume += volume;

		uint16 leftVolume = (volume) >> 4;
		uint16 rightVolume = (volume & 0x0F);

		uint16 frequency = (_soundSourceConfigurations[i].SxFQH << 4) | _soundSourceConfigurations[i].SxFQL;

		uint16 leftValue = 0;
		uint16 rightValue = 0;

		leftValue = ((frequency * leftVolume) / __MAXIMUM_VOLUME) >> 4;
		rightValue = ((frequency * rightVolume) / __MAXIMUM_VOLUME) >> 4;

		char boxesArray[] = 
		{
			__TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX,
			__TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX,
			__TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX,
			'C', '0' + i + 1,
			__TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX,
			__TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX,
			__TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX, __TILE_DARK_RED_BOX,
			'\0'
		};

		for(uint16 j = 0; j < leftValue && 15 > j; j++)
		{
			boxesArray[15 - j - 1] = __TILE_BRIGHT_RED_BOX;
		}

		for(uint16 j = 0; j < rightValue && 15 > j; j++)
		{
			boxesArray[15 + 2 + j] = __TILE_BRIGHT_RED_BOX;
		}

		PRINT_TEXT(boxesArray, x, y);

		y++;
	}
}
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::reset()
{
	SoundUnit::getInstance();

	if(NULL == _queuedSoundSourceConfigurationRequests)
	{
		_queuedSoundSourceConfigurationRequests = new VirtualList();	
	}

	_allowQueueingSoundRequests = true;
	_haveUsedSoundSources = false;
	_haveQueuedRequests = false;

	SoundUnit::stopAllSounds();
	SoundUnit::enableQueue();

	_ticks = 0;
	_haveUsedSoundSources = false;
	_haveQueuedRequests = false;

	VirtualList::deleteData(_queuedSoundSourceConfigurationRequests);

	for(int16 i = 0; i < __TOTAL_WAVEFORMS; i++)
	{
		_waveforms[i].index = i;
		_waveforms[i].usageCount = 0;
		_waveforms[i].wave = __WAVE_ADDRESS(i);
		_waveforms[i].data = NULL;
		_waveforms[i].crc = 0;

		for(uint32 j = 0; j < 128; j++)
		{
			_waveforms[i].wave[j] = 0;
		}
	}

	uint8* modulationData = __MODULATION_DATA;

	for(int16 i = 0; i <= __MODULATION_DATA_ENTRIES; i++)
	{
		modulationData[i] = 0;
	}

	for(int16 i = 0; i < __TOTAL_NORMAL_CHANNELS; i++)
	{
		_soundSourceConfigurations[i].type = kSoundSourceNormal;
	}

	for(int16 i = __TOTAL_NORMAL_CHANNELS; i < __TOTAL_NORMAL_CHANNELS + __TOTAL_MODULATION_CHANNELS; i++)
	{
		_soundSourceConfigurations[i].type = kSoundSourceModulation | kSoundSourceNormal;
	}

	for
	(
		int16 i = __TOTAL_NORMAL_CHANNELS + __TOTAL_MODULATION_CHANNELS; 
		i < __TOTAL_NORMAL_CHANNELS + __TOTAL_MODULATION_CHANNELS + __TOTAL_NOISE_CHANNELS; 
		i++
	)
	{
		_soundSourceConfigurations[i].type = kSoundSourceNoise;
	}

	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		_soundSourceConfigurations[i].requesterId = -1;
		_soundSourceConfigurations[i].soundSource = &_soundSources[i];
		_soundSourceConfigurations[i].waveform = NULL;
		_soundSourceConfigurations[i].timeout = 0;
		_soundSourceConfigurations[i].SxLRV = 0;
		_soundSourceConfigurations[i].SxFQL = 0;
		_soundSourceConfigurations[i].SxFQH = 0;
		_soundSourceConfigurations[i].SxEV0 = 0;
		_soundSourceConfigurations[i].SxEV1 = 0;
		_soundSourceConfigurations[i].SxRAM = 0;
		_soundSourceConfigurations[i].SxSWP = 0;
		_soundSourceConfigurations[i].SxINT = 0;
		_soundSourceConfigurations[i].priority = -1;
		_soundSourceConfigurations[i].waveform = NULL;
		_soundSourceConfigurations[i].soundSource->SxLRV = _soundSourceConfigurations[i].SxLRV;
		_soundSourceConfigurations[i].soundSource->SxFQL = _soundSourceConfigurations[i].SxFQL;
		_soundSourceConfigurations[i].soundSource->SxFQH = _soundSourceConfigurations[i].SxFQH;
		_soundSourceConfigurations[i].soundSource->SxEV0 = _soundSourceConfigurations[i].SxEV0;
		_soundSourceConfigurations[i].soundSource->SxEV1 = _soundSourceConfigurations[i].SxEV1;
		_soundSourceConfigurations[i].soundSource->SxRAM = 0;
		_soundSourceConfigurations[i].soundSource->SxSWP = _soundSourceConfigurations[i].SxSWP;
		_soundSourceConfigurations[i].soundSource->SxINT = _soundSourceConfigurations[i].SxINT;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::update()
{
	if(_haveUsedSoundSources)
	{
		SoundUnit::releaseSoundSources();
	}

	if(_haveQueuedRequests)
	{
		SoundUnit::dispatchQueuedSoundSourceConfigurations();
	}

	_ticks++;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::stopAllSounds()
{
	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		_soundSourceConfigurations[i].requesterId = -1;
		_soundSourceConfigurations[i].waveform = NULL;
		_soundSourceConfigurations[i].timeout = 0;
		_soundSourceConfigurations[i].SxLRV = 0;
		_soundSourceConfigurations[i].SxFQL = 0;
		_soundSourceConfigurations[i].SxFQH = 0;
		_soundSourceConfigurations[i].SxEV0 = 0;
		_soundSourceConfigurations[i].SxEV1 = 0;
		_soundSourceConfigurations[i].SxRAM = 0;
		_soundSourceConfigurations[i].SxSWP = 0;
		_soundSourceConfigurations[i].SxINT = 0;
		_soundSourceConfigurations[i].priority = -1;
	}

	__SSTOP = 0x01;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::enableQueue()
{
	_allowQueueingSoundRequests = true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::disableQueue()
{
	_allowQueueingSoundRequests = false;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::flushQueuedSounds()
{
	VirtualList::deleteData(_queuedSoundSourceConfigurationRequests);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::configureSoundSource
(
	const SoundSourceConfigurationRequest* soundSourceConfigurationRequest, SoundSourceEntry soundSourceEntry
)
{
	int16 i = soundSourceEntry.soundSourceIndex;
	SoundSource* soundSource = _soundSourceConfigurations[i].soundSource;

	_haveUsedSoundSources = true;

	bool setSxINT = 
		0 != (__SET_SxINT_FLAG & soundSourceConfigurationRequest->SxINT)
		|| 
		_soundSourceConfigurations[i].SxINT != soundSourceConfigurationRequest->SxINT
		|| 
		(_soundSourceConfigurations[i].requesterId != soundSourceConfigurationRequest->requesterId);

	bool setSxEV0 = 
		0 != (__SET_SxEV0_FLAG & soundSourceConfigurationRequest->SxEV1)
		||
		_soundSourceConfigurations[i].SxEV0 != soundSourceConfigurationRequest->SxEV0
		||
		(_soundSourceConfigurations[i].requesterId != soundSourceConfigurationRequest->requesterId);

	bool setSxEV1 = 
		0 != (__SET_SxEV1_FLAG & soundSourceConfigurationRequest->SxEV1)
		||
		_soundSourceConfigurations[i].SxEV1 != soundSourceConfigurationRequest->SxEV1
		|| 
		(_soundSourceConfigurations[i].requesterId != soundSourceConfigurationRequest->requesterId);

	bool setSxSWP =
		0 != (__SET_SxSWP_FLAG & soundSourceConfigurationRequest->SxEV1)
		|| 
		_soundSourceConfigurations[i].SxSWP != soundSourceConfigurationRequest->SxSWP
		||
		(_soundSourceConfigurations[i].requesterId != soundSourceConfigurationRequest->requesterId);

	_soundSourceConfigurations[i].requesterId = soundSourceConfigurationRequest->requesterId;
	_soundSourceConfigurations[i].waveform = soundSourceEntry.waveform;
	_soundSourceConfigurations[i].timeout = _ticks + soundSourceConfigurationRequest->timeout;
	_soundSourceConfigurations[i].SxINT = soundSourceConfigurationRequest->SxINT;
	_soundSourceConfigurations[i].SxLRV = soundSourceConfigurationRequest->SxLRV;
	_soundSourceConfigurations[i].SxFQL = soundSourceConfigurationRequest->SxFQL;
	_soundSourceConfigurations[i].SxFQH = soundSourceConfigurationRequest->SxFQH;
	_soundSourceConfigurations[i].SxEV0 = soundSourceConfigurationRequest->SxEV0;
	_soundSourceConfigurations[i].SxEV1 = soundSourceConfigurationRequest->SxEV1;
	_soundSourceConfigurations[i].SxRAM = soundSourceEntry.waveform->index;
	_soundSourceConfigurations[i].SxSWP = soundSourceConfigurationRequest->SxSWP;
	_soundSourceConfigurations[i].priority = soundSourceConfigurationRequest->priority;

	if(setSxINT)
	{
		soundSource->SxINT = soundSourceConfigurationRequest->SxINT;
	}

	if(setSxEV0)
	{
		soundSource->SxEV0 = soundSourceConfigurationRequest->SxEV0;
	}
	
	if(setSxEV1)
	{
		soundSource->SxEV1 = soundSourceConfigurationRequest->SxEV1;
	}

	soundSource->SxLRV = soundSourceConfigurationRequest->SxLRV;
	soundSource->SxFQL = soundSourceConfigurationRequest->SxFQL;
	soundSource->SxFQH = soundSourceConfigurationRequest->SxFQH;

	if(setSxSWP)
	{
		soundSource->SxSWP = soundSourceConfigurationRequest->SxSWP;
	}

	soundSource->SxRAM = soundSourceEntry.waveform->index;

	if(NULL != soundSourceConfigurationRequest->SxMOD)
	{		
		uint8* modulationData = __MODULATION_DATA;

		for(int16 i = 0; i <= __MODULATION_DATA_ENTRIES; i++)
		{
			modulationData[i << 2] = soundSourceConfigurationRequest->SxMOD[i];
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static SoundSourceEntry SoundUnit::findSoundSource(uint32 requesterId, uint32 soundSourceType, uint8 priority, const WaveformData* waveFormData)
{
	SoundSourceEntry soundSourceEntry = {-1, NULL};

	// First try to find a sound source that has previously assigned to the same requesterId
	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		if(0 == (soundSourceType & _soundSourceConfigurations[i].type))
		{
			continue;
		}

		if(requesterId == _soundSourceConfigurations[i].requesterId)
		{
			soundSourceEntry.soundSourceIndex = i;
			soundSourceEntry.waveform = SoundUnit::findWaveform(waveFormData);
		 	return soundSourceEntry;
		}
	}

	// Now try to find a sound source whose timeout has just expired
	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		if(0 == (soundSourceType & _soundSourceConfigurations[i].type))
		{
			continue;
		}

		if(_ticks >= _soundSourceConfigurations[i].timeout)
		{
			soundSourceEntry.soundSourceIndex = i;
			soundSourceEntry.waveform = SoundUnit::findWaveform(waveFormData);
			return soundSourceEntry;
		}
	}

	int16 stolenSoundSourceIndex = -1;

	// Now try to find a sound source we can steal from
	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		if(0 == (soundSourceType & _soundSourceConfigurations[i].type))
		{
			continue;
		}

		if(_soundSourceConfigurations[i].priority > priority)
		{
			continue;
		}

		if
		(
			0 > stolenSoundSourceIndex 
			|| 
			_soundSourceConfigurations[i].timeout < _soundSourceConfigurations[stolenSoundSourceIndex].timeout
		)
		{
			stolenSoundSourceIndex = i;
		}
	}		

	if(-1 < stolenSoundSourceIndex)
	{
		soundSourceEntry.soundSourceIndex = stolenSoundSourceIndex;
		soundSourceEntry.waveform = SoundUnit::findWaveform(waveFormData);
	}

	return soundSourceEntry;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::releaseSoundSources()
{
	_haveUsedSoundSources = false;

	for(int16 i = 0; i < __TOTAL_WAVEFORMS; i++)
	{
		_waveforms[i].usageCount = 0;
	}

	for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
	{
		if(0 == _soundSourceConfigurations[i].timeout)
		{
			continue;
		}

		/// Don't change to >= since it prevents pop sounds when a new sound request
		/// arrives in during the same timer interrupt as this.
		if(_ticks > _soundSourceConfigurations[i].timeout)
		{
			_soundSourceConfigurations[i].requesterId = -1;
			_soundSourceConfigurations[i].timeout = 0;
			_soundSourceConfigurations[i].waveform = NULL;
			_soundSourceConfigurations[i].SxINT |= __SOUND_WRAPPER_STOP_SOUND;
			_soundSourceConfigurations[i].soundSource->SxINT |= __SOUND_WRAPPER_STOP_SOUND;
			_soundSourceConfigurations[i].priority = -1;
		}
		else if(NULL != _soundSourceConfigurations[i].waveform)
		{
			_waveforms[_soundSourceConfigurations[i].waveform->index].usageCount++;
		
			_haveUsedSoundSources = true;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::dispatchQueuedSoundSourceConfigurations()
{
	if(isDeleted(_queuedSoundSourceConfigurationRequests) || NULL == _queuedSoundSourceConfigurationRequests->head)
	{
		return;
	}

	for(VirtualNode node = _queuedSoundSourceConfigurationRequests->head, nextNode = NULL; NULL != node; node = nextNode)
	{
		nextNode = node->next;

		SoundSourceConfigurationRequest* queuedSoundSourceConfigurationRequest = (SoundSourceConfigurationRequest*)node->data;

		SoundSourceEntry soundSourceEntry = 
			SoundUnit::findSoundSource
			(
				queuedSoundSourceConfigurationRequest->requesterId, queuedSoundSourceConfigurationRequest->type, 
				queuedSoundSourceConfigurationRequest->priority, queuedSoundSourceConfigurationRequest->SxRAM
			);

		if(0 <= soundSourceEntry.soundSourceIndex)
		{
			SoundUnit::configureSoundSource(queuedSoundSourceConfigurationRequest, soundSourceEntry);

			VirtualList::removeNode(_queuedSoundSourceConfigurationRequests, node);

			delete queuedSoundSourceConfigurationRequest;
		}
	}

	_haveQueuedRequests = NULL != _queuedSoundSourceConfigurationRequests->head;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::registerQueuedSoundSourceConfigurationRequest(const SoundSourceConfigurationRequest* soundSourceConfigurationRequest)
{
	if(NULL == soundSourceConfigurationRequest || isDeleted(_queuedSoundSourceConfigurationRequests))
	{
		return;
	}

	_haveQueuedRequests = true;

	SoundSourceConfigurationRequest* queuedSoundSourceConfigurationRequest = new SoundSourceConfigurationRequest;
	*queuedSoundSourceConfigurationRequest = *soundSourceConfigurationRequest;

	VirtualList::pushBack(_queuedSoundSourceConfigurationRequests, queuedSoundSourceConfigurationRequest);

}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static Waveform* SoundUnit::findWaveform(const WaveformData* waveFormData)
{
	if(NULL == waveFormData)
	{
		return NULL;
	}

	for(int16 i = 0; i < __TOTAL_WAVEFORMS; i++)
	{
		if(NULL != _waveforms[i].data && waveFormData->crc == _waveforms[i].crc)
		{
			_waveforms[i].usageCount = 1;

			return &_waveforms[i];
		}
	}

	for(int16 i = 0; i < __TOTAL_WAVEFORMS; i++)
	{
		if(NULL == _waveforms[i].data || 0 == _waveforms[i].usageCount)
		{
			SoundUnit::setWaveform(&_waveforms[i], waveFormData);

			return &_waveforms[i];
		}
	}

	/// Fallback
	return &_waveforms[0];
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void SoundUnit::setWaveform(Waveform* waveform, const WaveformData* waveFormData)
{
	if(NULL != waveform && NULL != waveFormData)
	{
		waveform->usageCount = 1;

		if(waveform->crc == waveFormData->crc) 
		{
			return;
		}

		waveform->data = waveFormData->data;
		waveform->crc = waveFormData->crc;

		// Disable interrupts to make the following as soon as possible
		Hardware::suspendInterrupts();

		// Must stop all sound sources before writing the waveforms
		__SSTOP = 0x01;

		// Set the wave data
		for(uint32 i = 0; i < 32; i++)
		{
			waveform->wave[(i << 2)] = (uint8)waveform->data[i];
		}

		for(int16 i = 0; i < __TOTAL_SOUND_SOURCES; i++)
		{
			if(0 < _soundSourceConfigurations[i].timeout)
			{
				SoundSource* soundSource = _soundSourceConfigurations[i].soundSource;

				soundSource->SxINT = _soundSourceConfigurations[i].SxINT | 0x80;
			}
		}

		// Turn back interrupts on
		Hardware::resumeInterrupts();
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SoundUnit::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void SoundUnit::destructor()
{
	if(!isDeleted(_queuedSoundSourceConfigurationRequests))
	{
		VirtualList::deleteData(_queuedSoundSourceConfigurationRequests);
		delete _queuedSoundSourceConfigurationRequests;
		_queuedSoundSourceConfigurationRequests = NULL;
	}

	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
