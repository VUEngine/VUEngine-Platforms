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

#include "ObjectSpriteContainer.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectSpriteContainer::constructor(Entity owner, const SpriteSpec* spriteSpec)
{
	// Always explicitly call the base's constructor 
	Base::constructor(owner, spriteSpec);

	this->sptBoundaryObjectIndex = __TOTAL_OBJECTS;
	this->hasTextures = false;
	this->head = __WORLD_ON | __WORLD_OBJECT | __WORLD_OVR;
	this->head &= ~__WORLD_END;
	this->transparency = __TRANSPARENCY_NONE;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectSpriteContainer::destructor()
{
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

ClassPointer ObjectSpriteContainer::getBasicType()
{
	return typeofclass(ObjectSpriteContainer);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectSpriteContainer::loadTexture(ClassPointer textureClass __attribute__((unused)))
{}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int16 ObjectSpriteContainer::doRender(int16 index)
{
	if(__TOTAL_OBJECTS <= this->sptBoundaryObjectIndex)
	{
		return 0;
	};

	this->index = index;

	// Force rendering
	this->rendered = false;

	_worldAttributesCache[index].head = (__WORLD_ON | __WORLD_OBJECT | __WORLD_OVR) & (~__WORLD_END);

	return 1;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int32 ObjectSpriteContainer::getTotalPixels()
{
	return 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectSpriteContainer::resetSPTBoundaryObjectIndex()
{
	this->sptBoundaryObjectIndex = __TOTAL_OBJECTS;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectSpriteContainer::setSPTBoundaryObjectIndex(int16 sptBoundaryObjectIndex)
{
	if(this->sptBoundaryObjectIndex > sptBoundaryObjectIndex)
	{	
		this->sptBoundaryObjectIndex = sptBoundaryObjectIndex;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int16 ObjectSpriteContainer::getSPTBoundaryObjectIndex()
{
	return this->sptBoundaryObjectIndex;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
