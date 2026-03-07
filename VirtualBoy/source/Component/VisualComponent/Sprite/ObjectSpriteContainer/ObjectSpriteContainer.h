/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#ifndef OBJECT_SPRITE_CONTAINER_H_
#define OBJECT_SPRITE_CONTAINER_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <Sprite.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATION
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Class ObjectSpriteContainer
///
/// Inherits from Sprite
///
/// Manages ObjectSprites that are displayed in the same SPT.
class ObjectSpriteContainer : Sprite
{
	/// @protectedsection
	int16 sptBoundaryObjectIndex;

	/// @publicsection

	/// Class' constructor
	void constructor();

	/// Retrieve the basic class of this kind of sprite.
	/// @return ClassPointer the basic class
	override ClassPointer getBasicType();

	/// Render the sprite by configuring the DRAM assigned to it by means of the provided index.
	/// @param index: Determines the region of DRAM that this sprite is allowed to configure
	/// @return The index that determines the region of DRAM that this sprite manages
	override int16 doRender(int16 index);
	
	/// Retrieve the total number of pixels actually displayed by all the managed sprites.
	/// @return Total number of pixels displayed by all the managed sprites
	override int32 getTotalPixels();

	/// Set the last object index used during rendering.
	/// @param sptBoundaryObjectIndex: Index of the last rendered object sprite
	void setSPTBoundaryObjectIndex(int16 sptBoundaryObjectIndex);

	/// Get the last object index used during rendering.
	/// @return Index of the last rendered object sprite
	int16 getSPTBoundaryObjectIndex();
}

#endif
