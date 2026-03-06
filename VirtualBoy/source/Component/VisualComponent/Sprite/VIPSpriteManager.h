/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#ifndef VIP_SPRITE_MANAGER_H_
#define VIP_SPRITE_MANAGER_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <ListenerObject.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// FORWARD DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

class Sprite;
class ObjectSpriteContainer;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATION
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Class VIPSpriteManager
///
/// Inherits from ListenerObject
///
/// Manages the VIP's hardware for sprites.
singleton class VIPSpriteManager : ListenerObject
{
	// Pointers to access the WORLD DRAM space
	WorldAttributes* worldAttributesBaseAddress;

	// Pointers to access the OBJECT DRAM space
	ObjectAttributes* objectAttributesBaseAddress;

	/// List of object sprite containers
	ObjectSpriteContainer objectSpriteContainers[__TOTAL_OBJECT_SEGMENTS];

	/// Cache for the VIP SPT registers
	uint16 vipSPTRegistersCache[__TOTAL_OBJECT_SEGMENTS];

	/// Free WORLD layer during the last rendering cycle
	const int16* bgmapIndex;

	/// Free OBJECT during the last rendering cycle
	const int16* objectIndex;

	/// Previous free OBJECT during the last rendering cycle, used to avoid
	/// writing the whole address space
	int16 previousObjectIndex;
	
	/// SPT index that for OBJECT memory management
	int8 spt;
		
	/// Configure the custom sprite manager
	/// @param paramTableSegments: Number of BGMAP segments reserved for the param tables
	/// @param objectSpritesContainersConfig: Configuration data for the object sprites
	void configure(int32 paramTableSegments, ObjectSpritesConfig objectSpritesConfig[__TOTAL_OBJECT_SEGMENTS]);

	/// Start rendering operations.
	void startRendering();

	/// Stop rendering operations.
	void stopRendering();

	/// Disable rendering operations.
	void disableRendering();

	/// Commit graphics to memory.
	void commitGraphics();

	/// Configure the number of available sprite slots per sprite lists
	/// @param availableSlots: Pointer to the array to fill
	/// @param nextSlotIndex: Pointer to the indexes of each slot for every sprite list 
	/// @param totalSpriteLists: Number of total sprite lists
	void fillAvailableSlots(int16* availableSlots, const int16** nextSlotIndex, int16 totalSpriteLists);

	/// Retrieve the index of the list to which the sprite belongs.
	/// @param sprite: Sprite to check
	/// @return The index of the list in which a sprite must be registered
	uint16 getSpriteListIndex(Sprite sprite);

	/// Print OBJECT related stats.
	/// @param x: Screen x coordinate where to print
	/// @param y: Screen y coordinate where to print
	void printSPTInfo(int16 spt, int32 x, int32 y);
}

#endif
