/*
 * VUEngine Core
 *
 * © Jorge Eremiev <jorgech3@gmail.com> and Christian Radke <c.radke@posteo.de>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#ifndef OBJECT_TEXTURE_MANAGER_H_
#define OBJECT_TEXTURE_MANAGER_H_

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// INCLUDES
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#include <ObjectTexture.h>
#include <TextureManager.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATION
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/// Class BgmapTextureManager
///
/// Inherits from Texture
///
/// Manages textures for OBJECT space.
singleton class ObjectTextureManager : TextureManager
{
	/// @publicsection

	/// Retrieve the class type of the managed textures.
	/// @return ClassPointer to the type of textures
	override ClassPointer getTextureClass();

	/// Erase the contents of graphics memory space.
	override void clear();

	/// Reset the manager's state.
	override void reset();

	/// Load textures in function of the provided array of specs.
	/// @param textureSpecs: Array of texture specs in function of which to load textures 
	override void load(const TextureSpec** textureSpecs);

	/// Retrieve a texture initialized with the provided spec.
	/// @param textureSpec: Spec to use to initilize the desired texture
	/// @return Texture initialized with the provided spec
	override Texture get(const TextureSpec* textureSpec);

	/// Release a texture.
	/// @param texture: Texture to release
	override void release(Texture texture);

}

#endif
