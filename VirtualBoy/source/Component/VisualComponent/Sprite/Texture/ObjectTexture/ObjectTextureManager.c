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

#include <ObjectTexture.h>
#include <Singleton.h>
#include <VirtualList.h>

#include "ObjectTextureManager.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#ifdef __RELEASE		
friend class TileSet;
#endif
friend class ObjectTexture;
friend class Texture;
friend class VirtualList;
friend class VirtualNode;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

secure ClassPointer ObjectTextureManager::getTextureClass()
{
	return typeofclass(ObjectTexture);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

secure void ObjectTextureManager::clear()
{}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

secure void ObjectTextureManager::reset()
{
	VirtualList::deleteData(this->textures);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

secure void ObjectTextureManager::load(const TextureSpec** textureSpecs __attribute__((unused)))
{}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

secure Texture ObjectTextureManager::get(const TextureSpec* textureSpec)
{
	NM_ASSERT(NULL != textureSpec, "ObjectTextureManager::getTexture: NULL textureSpec");

	if(NULL == textureSpec)
	{
		return NULL;
	}

	static uint16 textureNextId = 0;

	ObjectTexture objectTexture = new ObjectTexture((ObjectTextureSpec*)textureSpec, textureNextId++);

	VirtualList::pushBack(this->textures, objectTexture);
	
	ObjectTexture::prepare(objectTexture);

	return Texture::safeCast(objectTexture);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

secure void ObjectTextureManager::release(Texture texture)
{
	NM_ASSERT(!isDeleted(texture), "ObjectTextureManager::releaseTexture: trying to release an invalid objectTexture");

	VirtualList::removeData(this->textures, texture);

	delete texture;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectTextureManager::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();
	
	this->textures = new VirtualList();

	ObjectTextureManager::reset(this);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectTextureManager::destructor()
{
	if(!isDeleted(this->textures))
	{
		VirtualList::deleteData(this->textures);
		delete this->textures;
		this->textures = NULL;
	}

	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
