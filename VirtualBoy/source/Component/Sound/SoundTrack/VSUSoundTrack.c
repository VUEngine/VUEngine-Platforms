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

#include <SoundUnit.h>

#include "VSUSoundTrack.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VSUSoundTrack::reset()
{
	Base::reset(this);

	this->cursorSxINT = 0;
	this->cursorSxLRV = 0;
	this->cursorSxFQ = 0;
	this->cursorSxEV0 = 0;
	this->cursorSxEV1 = 0;
	this->cursorSxRAM = 0;
	this->cursorSxSWP = 0;
	this->cursorSxMOD = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VSUSoundTrack::updateCursors()
{
	SoundTrackKeyframe soundTrackKeyframe = this->soundTrackSpec->trackKeyframes[this->cursor];

	if(0 != (kSoundTrackEventSxINT & soundTrackKeyframe.events))
	{
		this->cursorSxINT++;
	}

	if(0 != (kSoundTrackEventSxLRV & soundTrackKeyframe.events))
	{
		this->cursorSxLRV++;
	}

	if(0 != (kSoundTrackEventSxFQ & soundTrackKeyframe.events))
	{
		this->cursorSxFQ++;
	}

	if(0 != (kSoundTrackEventSxEV0 & soundTrackKeyframe.events))
	{
		this->cursorSxEV0++;
	}

	if(0 != (kSoundTrackEventSxEV1 & soundTrackKeyframe.events))
	{
		this->cursorSxEV1++;
	}

	if(0 != (kSoundTrackEventSxRAM & soundTrackKeyframe.events))
	{
		this->cursorSxRAM++;
	}

	if(0 != (kSoundTrackEventSxSWP & soundTrackKeyframe.events))
	{
		this->cursorSxSWP++;
	}

	if(0 != (kSoundTrackEventSxMOD & soundTrackKeyframe.events))
	{
		this->cursorSxMOD++;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VSUSoundTrack::sendSoundRequest
(
	fix7_9_ext targetTimerResolutionFactor, uint8 maximumVolume, uint8 leftVolumeReduction, 
	uint8 rightVolumeReduction, uint8 volumeReduction, uint16 frequencyDelta
)
{
	uint8 volume = ((VSUSoundTrackSpec*)this->soundTrackSpec)->SxLRV[this->cursorSxLRV];
	
	int8 leftVolume = volume >> 4;
	int8 rightVolume = volume & 0xF;

	SoundTrackKeyframe soundTrackKeyframe = this->soundTrackSpec->trackKeyframes[this->cursor];

	if(0 < leftVolume)
	{
		if(leftVolume > maximumVolume)
		{
			leftVolume = maximumVolume;
		}
		
		leftVolume -= (volumeReduction + leftVolumeReduction);

		if(leftVolume <= 0)
		{
			// Don't allow 0 values to prevent pop sounds
			leftVolume = 1;
		}
	}

	if(0 < rightVolume)
	{
		if(rightVolume > maximumVolume)
		{
			rightVolume = maximumVolume;
		}

		rightVolume -= (volumeReduction + rightVolumeReduction);

		if(rightVolume <= 0)
		{
			// Don't allow 0 values to prevent pop sounds
			rightVolume = 1;
		}
	}

	if(0 < leftVolume || 0 < rightVolume)
	{
		uint16 note = ((VSUSoundTrackSpec*)this->soundTrackSpec)->SxFQ[this->cursorSxFQ] + frequencyDelta;

		SoundSourceConfigurationRequest vsuChannelConfigurationRequest = 
		{
			// Requester object
			this->id,
			// Time when the configuration elapses
			__FIX7_9_EXT_DIV(__I_TO_FIX7_9_EXT(soundTrackKeyframe.tick), targetTimerResolutionFactor),
			// Sound source type
			0 != (kSoundTrackEventSweepMod & soundTrackKeyframe.events) ? 
				kSoundSourceModulation:
				0 != (kSoundTrackEventNoise & soundTrackKeyframe.events) ?
					kSoundSourceNoise:
					kSoundSourceNormal,
			// SxINT values
			((VSUSoundTrackSpec*)this->soundTrackSpec)->SxINT[this->cursorSxINT],
			// SxLRV values
			(leftVolume << 4) | rightVolume,
			// SxFQL values
			note & 0xFF,
			// SxFQH values
			note >> 8,
			// SxEV0 values
			((VSUSoundTrackSpec*)this->soundTrackSpec)->SxEV0[this->cursorSxEV0],
			// SxEV1 values
			((VSUSoundTrackSpec*)this->soundTrackSpec)->SxEV1[this->cursorSxEV1],
			// SxRAM pointer
			((VSUSoundTrackSpec*)this->soundTrackSpec)->SxRAM[this->cursorSxRAM],
			// SxSWP values
			((VSUSoundTrackSpec*)this->soundTrackSpec)->SxSWP[this->cursorSxSWP],
			// SxMOD pointer
			((VSUSoundTrackSpec*)this->soundTrackSpec)->SxMOD[this->cursorSxMOD],
			// Priority
			this->soundTrackSpec->priority,
			// Skip
			this->soundTrackSpec->skip
		};

		SoundUnit::applySoundSourceConfiguration(&vsuChannelConfigurationRequest);		
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VSUSoundTrack::constructor(const VSUSoundTrackSpec* vusSoundTrackSpec)
{
	// Always explicitly call the base's constructor 
	Base::constructor(&vusSoundTrackSpec->soundTrackSpec);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VSUSoundTrack::destructor()
{	
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
