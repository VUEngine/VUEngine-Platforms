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

#include <BgmapSprite.h>
#include <BgmapTextureManager.h>
#include <CharSet.h>
#include <ComponentManager.h>
#ifdef __DEBUG_TOOL
#include <Debug.h>
#endif
#include <DebugConfig.h>
#include <Hardware.h>
#include <Mem.h>
#include <ObjectSprite.h>
#include <ObjectSpriteContainer.h>
#include <ParamTableManager.h>
#include <Printer.h>
#include <Profiler.h>
#include <Singleton.h>
#include <VirtualList.h>
#include <VirtualNode.h>
#include <VUEngine.h>

#include "VIPSpriteManager.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DECLARATIONS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

friend class Sprite;
friend class VirtualNode;
friend class VirtualList;

extern volatile uint16* _vipRegisters;

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' DATA
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

enum SpriteListTypes
{
	kSpriteListObject = 0,
	kSpriteListBgmap
};

static const SpriteSpec ObjectSpriteContainerSpec =
{
	// VisualComponent
	{
		// Component
		{
			// Allocator
			__TYPE(ObjectSpriteContainer),

			// Component type
			kSpriteComponent
		},

		// Array of function animations
		(const AnimationFunction**)NULL
	},

	// Spec for the texture to display
	(TextureSpec*)NULL,

	// Transparency mode (__TRANSPARENCY_NONE, __TRANSPARENCY_EVEN or __TRANSPARENCY_ODD)
	__TRANSPARENCY_NONE,

	// Displacement added to the sprite's position
	{
		0, // x
		0, // y
		0, // z
		0, // parallax
	}
};

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PUBLIC METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::configure(int32 paramTableSegments, ObjectSpritesConfig objectSpritesConfig[__TOTAL_OBJECT_SEGMENTS])
{
	BgmapTextureManager::configure
	(
		BgmapTextureManager::getInstance(),
		ParamTableManager::configure
		(
			ParamTableManager::getInstance(), paramTableSegments
		)
	);

	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		if(!isDeleted(this->objectSpriteContainers[i]))
		{
			ComponentManager::destroyComponent(NULL, Component::safeCast(this->objectSpriteContainers[i]));
			this->objectSpriteContainers[i] = NULL;
		}
	}

	VIPSpriteManager::configureObjectSprites(this, objectSpritesConfig);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::startRendering()
{
	ParamTableManager::defragment(ParamTableManager::getInstance(), true);

	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		if(!isDeleted(this->objectSpriteContainers[i]))
		{
			ObjectSpriteContainer::resetSPTBoundaryObjectIndex(this->objectSpriteContainers[i]);
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::stopRendering()
{
	int16 spt = __TOTAL_OBJECT_SEGMENTS - 1;

	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		if(!isDeleted(this->objectSpriteContainers[i]))
		{
			int16 sptBoundaryObjectIndex = ObjectSpriteContainer::getSPTBoundaryObjectIndex(this->objectSpriteContainers[i]);

			if(__TOTAL_OBJECTS - 1 == sptBoundaryObjectIndex)
			{
				int16 index = ObjectSpriteContainer::getIndex(this->objectSpriteContainers[i]);

				if(__NO_RENDER_INDEX != index)
				{
					_worldAttributesCache[index].head = __WORLD_OFF;
				}
			}
			else
			{
				this->vipSPTRegistersCache[i] = sptBoundaryObjectIndex;

				// Make sure that the rest of spt segments only run up to the last
				// Used object index
				for(int32 i = spt--; i--;)
				{
					this->vipSPTRegistersCache[i] = sptBoundaryObjectIndex;
				}				
			}
		}
	}

#ifdef __ALERT_WORLD_MEMORY_DEPLETION
	NM_ASSERT(-1 <= (int16)(*this->objectIndex), "SpriteManager::stopRendering: no more WORLDS");
	NM_ASSERT(-1 <= (int16)(*this->bgmapIndex), "SpriteManager::stopRendering: no more OBJECTS");
#endif

	if(0 <= *this->bgmapIndex)
	{
		_worldAttributesCache[*this->bgmapIndex].head = __WORLD_END;
	}

	// Clear OBJ memory
	for(int32 i = *this->objectIndex; this->previousObjectIndex <= i; i--)
	{
		_objectAttributesCache[i].head = __OBJECT_SPRITE_CHAR_HIDE_MASK;
	}

	this->previousObjectIndex = *this->objectIndex;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::disableRendering()
{
	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		this->vipSPTRegistersCache[i] = __TOTAL_OBJECTS - 1;

		if(!isDeleted(this->objectSpriteContainers[i]))
		{
			ComponentManager::destroyComponent(NULL, Component::safeCast(this->objectSpriteContainers[i]));
			this->objectSpriteContainers[i] = NULL;
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::commitGraphics()
{
#ifdef __DEBUGGING_SPRITES
	_writtenObjectTiles = __TOTAL_OBJECTS - *this->objectIndex;
#endif

	for(int32 i = __TOTAL_OBJECT_SEGMENTS; i--;)
	{
		_vipRegisters[__SPT0 + i] = this->vipSPTRegistersCache[i] - *this->objectIndex;
	}

	WorldAttributes* worldAttributesBaseAddress = (WorldAttributes*)__WORLD_SPACE_BASE_ADDRESS;
	ObjectAttributes* objectAttributesBaseAddress = (ObjectAttributes*)__OBJECT_SPACE_BASE_ADDRESS;

	CACHE_RESET;

	Mem::copyWORD
	(
		(uint32*)(objectAttributesBaseAddress), (uint32*)(_objectAttributesCache + *this->objectIndex), 
		sizeof(ObjectAttributes) * (__TOTAL_OBJECTS - *this->objectIndex) >> 2
	);

	Mem::copyWORD
	(
		(uint32*)(worldAttributesBaseAddress + *this->bgmapIndex), (uint32*)(_worldAttributesCache + *this->bgmapIndex), 
		sizeof(WorldAttributes) * (__TOTAL_WORLD_LAYERS - (*this->bgmapIndex)) >> 2
	);
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::fillAvailableSlots(int16* availableSlots, const int16** nextSlotIndex, int16 totalSpriteLists __attribute__((unused)))
{
	this->bgmapIndex = nextSlotIndex[kSpriteListBgmap];
	availableSlots[kSpriteListBgmap] = __TOTAL_WORLD_LAYERS;
	
	this->objectIndex = nextSlotIndex[kSpriteListObject];	
	availableSlots[kSpriteListObject] = __TOTAL_OBJECTS;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

uint16 VIPSpriteManager::getSpriteListIndex(Sprite sprite)
{
	ClassPointer classPointer = Sprite::getBasicType(sprite);
	
	if(typeofclass(ObjectSprite) == classPointer)
	{
		int16 z = 0;

		if(NULL != sprite->transformation)
		{
			z = __METERS_TO_PIXELS(sprite->transformation->position.z);
		}

		ObjectSprite::setObjectSpriteContainer
		(
			ObjectSprite::safeCast(sprite), VIPSpriteManager::getObjectSpriteContainer(this, z + sprite->displacement.z)
		);
		
		return kSpriteListObject;
	}

	return kSpriteListBgmap;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

/*
void VIPSpriteManager::printSPTInfo(int16 spt, int32 x, int32 y)
{
	int32 totalUsedObjects = 0;
	int32 totalPixels = 0;

	for(int16 i = kSpriteListObject; i < kSpriteListObject + __TOTAL_OBJECT_SEGMENTS; i++)
	{
		for(VirtualNode node = this->spriteRegistry[i].sprites->head; NULL != node; node = node->next)
		{
			ObjectSprite objectSprite = ObjectSprite::safeCast(node->data);

			if(objectSprite->deleteMe)
			{
				continue;
			}

			totalUsedObjects += objectSprite->totalObjects;
		}
	}
	
	Printer::text("Total used objects: ", x, ++y, NULL);
	Printer::int32(totalUsedObjects, x + 20, y++, NULL);

	if(__TOTAL_OBJECT_SEGMENTS <= (unsigned)spt)
	{
		return;
	}

	ObjectSpriteContainer objectSpriteContainer = this->objectSpriteContainers[spt];

	y++;

	Printer::text("OBJECT ", x, y++, NULL);

	if(NULL != objectSpriteContainer)
	{
#ifdef __TOOLS
		SpriteManager::hideAllSprites(this, Sprite::safeCast(objectSpriteContainer), false);
#endif

		for(VirtualNode node = this->spriteRegistry[spt + kSpriteListObject].sprites->head; NULL != node; node = node->next)
		{
			ObjectSprite objectSprite = ObjectSprite::safeCast(node->data);

			if(objectSprite->deleteMe)
			{
				continue;
			}

			ObjectSprite::show(objectSprite);
		}

		Timer::wait(40);
	}

	totalUsedObjects = 0;
	totalPixels = 0;

	int16 firstObjectIndex = -1;
	int16 lastObjectIndex = -1;

	for(VirtualNode node = this->spriteRegistry[spt + kSpriteListObject].sprites->head; NULL != node; node = node->next)
	{
		ObjectSprite objectSprite = ObjectSprite::safeCast(node->data);

		if(objectSprite->deleteMe || __NO_RENDER_INDEX == objectSprite->index)
		{
			continue;
		}

		totalUsedObjects += objectSprite->totalObjects;
		totalPixels += ObjectSprite::getTotalPixels(objectSprite);

		if(0 > firstObjectIndex)
		{
			firstObjectIndex = objectSprite->index;
		}

		lastObjectIndex = objectSprite->index - objectSprite->totalObjects;
	}

	Printer::text("Index: ", x, ++y, NULL);
	Printer::int32(NULL != objectSpriteContainer ? objectSpriteContainer->index : -1, x + 18, y, NULL);
	Printer::text("Class: ", x, ++y, NULL);
	Printer::text(NULL != objectSpriteContainer ? __GET_CLASS_NAME(objectSpriteContainer) : "N/A", x + 18, y, NULL);
	Printer::text("Mode:", x, ++y, NULL);
	Printer::text("OBJECT   ", x + 18, y, NULL);
	Printer::text("Segment:                ", x, ++y, NULL);
	Printer::int32(spt, x + 18, y++, NULL);
	Printer::text("SPT value:                ", x, y, NULL);
	Printer::int32(NULL != objectSpriteContainer ? _vipRegisters[__SPT0 + spt] : 0, x + 18, y, NULL);
	Printer::text("HEAD:                   ", x, ++y, NULL);
	Printer::hex(this->worldAttributesBaseAddress[objectSpriteContainer->index].head, x + 18, y, 4, NULL);
	Printer::text("Total OBJs:            ", x, ++y, NULL);
	Printer::int32(totalUsedObjects, x + 18, y, NULL);
	Printer::text("OBJ index range:      ", x, ++y, NULL);
	Printer::int32(lastObjectIndex, x + 18, y, NULL);
	Printer::text("-", x  + 18 + 0 <= firstObjectIndex ? Math::getDigitsCount(firstObjectIndex) : 0, y, NULL);
	Printer::int32(firstObjectIndex, x  + 18 + Math::getDigitsCount(firstObjectIndex) + 1, y, NULL);
	Printer::text("Z Position: ", x, ++y, NULL);
	Printer::int32(NULL != objectSpriteContainer ? objectSpriteContainer->position.z : 0, x + 18, y, NULL);
	Printer::text("Pixels: ", x, ++y, NULL);
	Printer::int32(totalPixels, x + 18, y, NULL);
}
*/

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// CLASS' PRIVATE METHODS
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::constructor()
{
	// Always explicitly call the base's constructor 
	Base::constructor();

	this->bgmapIndex = NULL;
	this->objectIndex = NULL;
	this->previousObjectIndex = __TOTAL_OBJECTS - 1;

	// Pointers to access the DRAM space
	this->worldAttributesBaseAddress = (WorldAttributes*)__WORLD_SPACE_BASE_ADDRESS;
	this->objectAttributesBaseAddress = (ObjectAttributes*)__OBJECT_SPACE_BASE_ADDRESS;
	
	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		this->vipSPTRegistersCache[i] = __TOTAL_OBJECTS - 1;
		this->objectSpriteContainers[i] = NULL;
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::destructor()
{
	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		if(!isDeleted(this->objectSpriteContainers[i]))
		{
			ComponentManager::destroyComponent(NULL, Component::safeCast(this->objectSpriteContainers[i]));
			this->objectSpriteContainers[i] = NULL;
		}
	}
	
	// Always explicitly call the base's destructor 
	Base::destructor();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void VIPSpriteManager::configureObjectSprites
(
	const ObjectSpritesConfig objectSpritesConfig[__TOTAL_OBJECT_SEGMENTS]
)
{
#ifndef __RELEASE
	int16 previousZ = objectSpritesConfig[__TOTAL_OBJECT_SEGMENTS - 1].zPosition;
#endif

	for(int32 i = __TOTAL_OBJECT_SEGMENTS; i--; )
	{
		if(objectSpritesConfig[i].instantiate)
		{
			NM_ASSERT(objectSpritesConfig[i].zPosition <= previousZ, "VIPSpriteManager::configureObjectSprites: wrong z");
			NM_ASSERT(isDeleted(this->objectSpriteContainers[i]), "VIPSpriteManager::configureObjectSprites: container already created");

			if(!isDeleted(this->objectSpriteContainers[i]))
			{
				ComponentManager::destroyComponent(NULL, Component::safeCast(this->objectSpriteContainers[i]));
				this->objectSpriteContainers[i] = NULL;
			}
			
			this->objectSpriteContainers[i] = ObjectSpriteContainer::safeCast
			(
				ComponentManager::createComponent(NULL, (ComponentSpec*)&ObjectSpriteContainerSpec)
			);
	
			NM_ASSERT(!isDeleted(this->objectSpriteContainers[i]), "VIPSpriteManager::configureObjectSprites: error creating container");

			PixelVector position =
			{
				0, 0, objectSpritesConfig[i].zPosition, 0
			};

			ObjectSpriteContainer::setPosition(this->objectSpriteContainers[i], &position);

#ifndef __RELEASE
			previousZ = objectSpritesConfig[i].zPosition;
#endif
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

ObjectSpriteContainer VIPSpriteManager::getObjectSpriteContainer(fixed_t z)
{
	ObjectSpriteContainer objectSpriteContainer = NULL;

	for(int16 i = 0; i < __TOTAL_OBJECT_SEGMENTS; i++)
	{
		if(NULL == this->objectSpriteContainers[i])
		{
			continue;
		}

		if(NULL == objectSpriteContainer)
		{
			objectSpriteContainer = this->objectSpriteContainers[i];
		}
		else
		{
			if
			(
				__ABS(Sprite::getPosition(this->objectSpriteContainers[i])->z - z)
					< 
				__ABS(Sprite::getPosition(objectSpriteContainer)->z - z)
			)
			{
				objectSpriteContainer = this->objectSpriteContainers[i];
			}
		}
	}

	return objectSpriteContainer;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
