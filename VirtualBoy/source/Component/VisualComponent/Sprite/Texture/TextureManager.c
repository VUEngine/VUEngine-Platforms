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

#include <BgmapTextureManager.h>
#include <ObjectTextureManager.h>
#include <Singleton.h>

#include <TextureManager.h>


//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::reset()
{
	BgmapTextureManager::reset(BgmapTextureManager::getInstance());
	ObjectTextureManager::reset(ObjectTextureManager::getInstance());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static Texture TextureManager::get
(
	ClassPointer textureClass, const TextureSpec* textureSpec
)
{
	if(NULL == textureSpec)
	{
		return NULL;
	}

	if(typeofclass(BgmapTexture) == textureClass)
	{
		return 
			Texture::safeCast
			(
				BgmapTextureManager::getTexture
				(
					BgmapTextureManager::getInstance(), (BgmapTextureSpec*)textureSpec, 0, false, 0
				)
			);
	}
	else if(typeofclass(ObjectTexture) == textureClass)
	{
		return Texture::safeCast(ObjectTextureManager::getTexture(ObjectTextureManager::getInstance(), (ObjectTextureSpec*)textureSpec));
	}

	return NULL;	
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::release(Texture texture)
{
	if(__IS_INSTANCE_OF(BgmapTexture, texture))
	{
		BgmapTextureManager::releaseTexture(BgmapTextureManager::getInstance(), BgmapTexture::safeCast(texture));
	}
	else if(__IS_INSTANCE_OF(ObjectTexture, texture))
	{
		ObjectTextureManager::releaseTexture(ObjectTextureManager::getInstance(), ObjectTexture::safeCast(texture));
	}	
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::updateTextures(int16 maximumTextureRowsToWrite, bool defer)
{
	BgmapTextureManager::updateTextures(BgmapTextureManager::getInstance(), maximumTextureRowsToWrite, defer);
	ObjectTextureManager::updateTextures(ObjectTextureManager::getInstance(), maximumTextureRowsToWrite, defer);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::loadTextures(const TextureSpec** textureSpecs)
{
	BgmapTextureManager::loadTextures
	(
		BgmapTextureManager::getInstance(), textureSpecs
	);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::clearGraphicMemory()
{
	BgmapTextureManager::clearDRAM(BgmapTextureManager::getInstance());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
