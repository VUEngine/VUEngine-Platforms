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

	this->internalUsedSlots = 0;
	this->hasTextures = false;
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
	if(0 == this->internalUsedSlots)
	{
		return 0;
	};

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

void ObjectSpriteContainer::resetUsedSlots()
{
	this->internalUsedSlots = 0;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void ObjectSpriteContainer::registerUsedSlots(int16 usedSlots)
{
	this->internalUsedSlots += usedSlots;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

int16 ObjectSpriteContainer::getUsedSlots()
{
	return this->internalUsedSlots;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
