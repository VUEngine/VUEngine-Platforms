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
#include <Texture.h>
#include <TileSet.h>
#include <VirtualList.h>
#include <VirtualNode.h>

#include <TextureManager.h>

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

friend class Texture;
friend class TileSet;
friend class VirtualNode;
friend class VirtualList;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static BgmapTextureManager _bgmapTextureManager = NULL;
static ObjectTextureManager _objectTextureManager = NULL;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::reset()
{
	TextureManager::loadManagers();

	BgmapTextureManager::reset(_bgmapTextureManager);
	ObjectTextureManager::reset(_objectTextureManager);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static Texture TextureManager::get(ClassPointer textureClass, const TextureSpec* textureSpec)
{
	TextureManager::loadManagers();

	if(NULL == textureSpec)
	{
		return NULL;
	}

	if(typeofclass(BgmapTexture) == textureClass)
	{		
		BgmapTexture bgmapTexture = BgmapTextureManager::getTexture(_bgmapTextureManager, (BgmapTextureSpec*)textureSpec, 0, false, 0);

		if(!isDeleted(bgmapTexture))
		{
			return Texture::safeCast(bgmapTexture);
		}
	}
	else if(typeofclass(ObjectTexture) == textureClass)
	{
		ObjectTexture objectTexture = ObjectTextureManager::getTexture(_objectTextureManager, (ObjectTextureSpec*)textureSpec);

		if(!isDeleted(objectTexture))
		{
			return Texture::safeCast(objectTexture);
		}
	}

	return NULL;	
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::release(Texture texture)
{
	TextureManager::loadManagers();

	if(__IS_INSTANCE_OF(BgmapTexture, texture))
	{
		BgmapTextureManager::releaseTexture(_bgmapTextureManager, BgmapTexture::safeCast(texture));
	}
	else if(__IS_INSTANCE_OF(ObjectTexture, texture))
	{
		ObjectTextureManager::releaseTexture(_objectTextureManager, ObjectTexture::safeCast(texture));
	}	
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::updateTextures(int16 maximumTextureRowsToWrite, bool defer)
{
	TextureManager::loadManagers();

	void updateTextures(VirtualList textures, int16 maximumTextureRowsToWrite, bool defer)
	{
		if(NULL == textures)
		{
			return;
		}

		for(VirtualNode node = textures->head; NULL != node; node = node->next)
		{
			Texture texture = Texture::safeCast(node->data);
			
			if(kTextureInvalid == texture->status)
			{
				continue;
			}

			if(NULL != texture->tileSet && kTexturePendingWriting < texture->status)
			{
				texture->status = texture->generation != texture->tileSet->generation? kTexturePendingRewriting : texture->status;
			}

			if(kTextureWritten == texture->status)
			{
				continue;
			}

			if(kTextureWritten != Texture::update(texture, maximumTextureRowsToWrite) && defer)
			{
				break;
			}
		}
	}

	BgmapTextureManager::updateTextures(_bgmapTextureManager, updateTextures, maximumTextureRowsToWrite, defer);
	ObjectTextureManager::updateTextures(_objectTextureManager, updateTextures, maximumTextureRowsToWrite, defer);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::loadTextures(const TextureSpec** textureSpecs)
{
	TextureManager::loadManagers();

	BgmapTextureManager::loadTextures(_bgmapTextureManager, textureSpecs);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::clearGraphicMemory()
{
	TextureManager::loadManagers();
	
	BgmapTextureManager::clearDRAM(_bgmapTextureManager);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE STATIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

static void TextureManager::loadManagers()
{
	if(NULL == _bgmapTextureManager)
	{
		_bgmapTextureManager = BgmapTextureManager::getInstance();
	}

	if(NULL == _objectTextureManager)
	{
		_objectTextureManager = ObjectTextureManager::getInstance();
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
