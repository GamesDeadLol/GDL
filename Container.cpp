
#include "StdAfx.h"
#include "Container.h"
#include "World.h"
#include "ObjectMsgs.h"
#include "ChatMsgs.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "WorldLandBlock.h"

CContainerWeenie::CContainerWeenie()
{
	for (DWORD i = 0; i < MAX_WIELDED_COMBAT; i++)
		m_WieldedCombat[i] = NULL;
}

CContainerWeenie::~CContainerWeenie()
{
	for (DWORD i = 0; i < MAX_WIELDED_COMBAT; i++)
		m_WieldedCombat[i] = NULL;

	for (auto item : m_Wielded)
	{
		g_pWorld->RemoveEntity(item);
	}

	m_Wielded.clear();

	for (auto item : m_Items)
	{
		g_pWorld->RemoveEntity(item);
	}

	m_Items.clear();

	for (auto container : m_Packs)
	{
		g_pWorld->RemoveEntity(container);
	}

	m_Packs.clear();
}

void CContainerWeenie::ApplyQualityOverrides()
{
	CWeenieObject::ApplyQualityOverrides();

	if (m_Qualities.GetInt(ITEM_TYPE_INT, 0) & TYPE_CONTAINER)
	{
		if (GetItemsCapacity() < 0)
		{
			m_Qualities.SetInt(ITEMS_CAPACITY_INT, 120);
		}

		if (GetContainersCapacity() < 0)
		{
			m_Qualities.SetInt(CONTAINERS_CAPACITY_INT, 10);
		}
	}
}

void CContainerWeenie::PostSpawn()
{
	CWeenieObject::PostSpawn();

	m_bInitiallyLocked = IsLocked();
}

int CContainerWeenie::GetItemsCapacity()
{
	return InqIntQuality(ITEMS_CAPACITY_INT, 0);
}

int CContainerWeenie::GetContainersCapacity()
{
	return InqIntQuality(CONTAINERS_CAPACITY_INT, 0);
}

CContainerWeenie *CContainerWeenie::FindContainer(DWORD container_id)
{
	if (GetID() == container_id)
		return this;

	for (auto pack : m_Packs)
	{
		if (pack->GetID() == container_id)
		{
			if (CContainerWeenie *packContainer = pack->AsContainer())
			{
				return packContainer;
			}
		}
	}

	if (CWeenieObject *externalObject = g_pWorld->FindObject(container_id))
	{
		if (CContainerWeenie *externalContainer = externalObject->AsContainer())
		{
			if (externalContainer->_openedBy == GetTopLevelID())
			{
				return externalContainer;
			}
		}
	}

	return NULL;
}

CWeenieObject *CContainerWeenie::GetWieldedCombat(COMBAT_USE combatUse)
{
	// The first entry is "Undef" so we omit that.
	int index = combatUse - 1;

	if (index < 0 || index >= MAX_WIELDED_COMBAT)
		return NULL;

	return m_WieldedCombat[index];
}

void CContainerWeenie::SetWieldedCombat(CWeenieObject *wielded, COMBAT_USE combatUse)
{
	// The first entry is "Undef" so we omit that.
	int index = combatUse - 1;

	if (index < 0 || index >= MAX_WIELDED_COMBAT)
		return;

	m_WieldedCombat[index] = wielded;
}

CWeenieObject *CContainerWeenie::GetWieldedMelee()
{
	return GetWieldedCombat(COMBAT_USE_MELEE);
}

CWeenieObject *CContainerWeenie::GetWieldedMissile()
{
	return GetWieldedCombat(COMBAT_USE_MISSILE);
}

CWeenieObject *CContainerWeenie::GetWieldedAmmo()
{
	return GetWieldedCombat(COMBAT_USE_AMMO);
}

CWeenieObject *CContainerWeenie::GetWieldedShield()
{
	return GetWieldedCombat(COMBAT_USE_SHIELD);
}

CWeenieObject *CContainerWeenie::GetWieldedTwoHanded()
{
	return GetWieldedCombat(COMBAT_USE_TWO_HANDED);
}

CWeenieObject *CContainerWeenie::GetWieldedCaster()
{
	for (auto item : m_Wielded)
	{
		if (item->AsCaster())
			return item;
	}

	return NULL;
}

void CContainerWeenie::Container_GetWieldedByMask(std::list<CWeenieObject *> &wielded, DWORD inv_loc_mask)
{
	for (auto item : m_Wielded)
	{
		if (item->InqIntQuality(CURRENT_WIELDED_LOCATION_INT, 0, TRUE) & inv_loc_mask)
			wielded.push_back(item);
	}
}

void CContainerWeenie::ReleaseContainedItemRecursive(CWeenieObject *item)
{
	if (!item)
		return;

	for (DWORD i = 0; i < MAX_WIELDED_COMBAT; i++)
	{
		if (item == m_WieldedCombat[i])
			m_WieldedCombat[i] = NULL;
	}

	for (std::vector<CWeenieObject *>::iterator equipmentIterator = m_Wielded.begin(); equipmentIterator != m_Wielded.end();)
	{
		if (*equipmentIterator != item)
		{
			equipmentIterator++;
			continue;
		}

		equipmentIterator = m_Wielded.erase(equipmentIterator);
	}

	for (std::vector<CWeenieObject *>::iterator itemIterator = m_Items.begin(); itemIterator != m_Items.end();)
	{
		if (*itemIterator != item)
		{
			itemIterator++;
			continue;
		}
		
		itemIterator = m_Items.erase(itemIterator);
	}

	for (std::vector<CWeenieObject *>::iterator packIterator = m_Packs.begin(); packIterator != m_Packs.end();)
	{
		CWeenieObject *pack = *packIterator;

		if (pack != item)
		{
			pack->ReleaseContainedItemRecursive(item);
			packIterator++;
			continue;
		}

		packIterator = m_Packs.erase(packIterator);
	}

	if (item->GetContainerID() == GetID())
	{
		item->m_Qualities.SetInstanceID(CONTAINER_IID, 0);
	}

	if (item->GetWielderID() == GetID())
	{
		item->m_Qualities.SetInstanceID(WIELDER_IID, 0);
	}

	item->RecacheHasOwner();
}

BOOL CContainerWeenie::Container_CanEquip(CWeenieObject *item, DWORD location)
{
	if (!item)
		return FALSE;

	if (!item->IsValidWieldLocation(location))
		return FALSE;

	for (auto wielded : m_Wielded)
	{
		if (wielded == item)
			return TRUE;

		if (!wielded->CanEquipWith(item, location))
			return FALSE;
	}

	return TRUE;
}

void CContainerWeenie::Container_EquipItem(DWORD dwCell, CWeenieObject *item, DWORD inv_loc, DWORD child_location, DWORD placement)
{
	if (int combatUse = item->InqIntQuality(COMBAT_USE_INT, 0, TRUE))
		SetWieldedCombat(item, (COMBAT_USE)combatUse);

	bool bAlreadyEquipped = false;
	for (auto entry : m_Wielded)
	{
		if (entry == item)
		{
			bAlreadyEquipped = true;
			break;
		}
	}
	if (!bAlreadyEquipped)
	{
		m_Wielded.push_back(item);
	}

	if (child_location && placement)
	{
		item->m_Qualities.SetInt(PARENT_LOCATION_INT, child_location);
		item->set_parent(this, child_location);
		item->SetPlacementFrame(placement, FALSE);

		if (m_bWorldIsAware)
		{
			if (CWeenieObject *owner = GetWorldTopLevelOwner())
			{
				if (owner->GetBlock())
				{
					owner->GetBlock()->ExchangePVS(item, 0);
				}
			}

			/*
			BinaryWriter *writer = item->CreateMessage();
			g_pWorld->BroadcastPVS(dwCell, writer->GetData(), writer->GetSize(), OBJECT_MSG, 0, FALSE);
			delete writer;
			*/

			BinaryWriter Blah;
			Blah.Write<DWORD>(0xF749);
			Blah.Write<DWORD>(GetID());
			Blah.Write<DWORD>(item->GetID());
			Blah.Write<DWORD>(child_location);
			Blah.Write<DWORD>(placement);
			Blah.Write<WORD>(GetPhysicsObj()->_instance_timestamp);
			Blah.Write<WORD>(++item->_position_timestamp);
			g_pWorld->BroadcastPVS(dwCell, Blah.GetData(), Blah.GetSize());
		}
	}
	else
	{
		if (m_bWorldIsAware)
		{
			item->_position_timestamp++;

			BinaryWriter Blah;
			Blah.Write<DWORD>(0xF74A);
			Blah.Write<DWORD>(item->GetID());
			Blah.Write<WORD>(item->_instance_timestamp);
			Blah.Write<WORD>(item->_position_timestamp);
			g_pWorld->BroadcastPVS(dwCell, Blah.GetData(), Blah.GetSize());
		}
	}
}

CWeenieObject *CContainerWeenie::FindContainedItem(DWORD object_id)
{
	for (auto item : m_Wielded)
	{
		if (item->GetID() == object_id)
			return item;
	}

	for (auto item : m_Items)
	{
		if (item->GetID() == object_id)
			return item;
	}

	for (auto item : m_Packs)
	{
		if (item->GetID() == object_id)
			return item;
		
		if (auto subitem = item->FindContainedItem(object_id))
			return subitem;
	}

	return NULL;
}

DWORD CContainerWeenie::Container_GetNumFreeMainPackSlots()
{
	return (DWORD) max(0, GetItemsCapacity() - (signed)m_Items.size());
}

BOOL CContainerWeenie::Container_CanStore(CWeenieObject *pItem)
{
	return Container_CanStore(pItem, pItem->RequiresPackSlot());
}

BOOL CContainerWeenie::IsItemsCapacityFull()
{
	int capacity = GetItemsCapacity();

	if (capacity >= 0)
	{
		if (m_Items.size() < capacity)
			return TRUE;

		return FALSE;
	}

	return TRUE;
}

BOOL CContainerWeenie::IsContainersCapacityFull()
{
	int capacity = GetContainersCapacity();

	if (capacity >= 0)
	{
		if (m_Packs.size() < capacity)
			return TRUE;

		return FALSE;
	}

	return TRUE;
}

BOOL CContainerWeenie::Container_CanStore(CWeenieObject *pItem, bool bPackSlot)
{
	// TODO handle: pItem->InqBoolQuality(REQUIRES_BACKPACK_SLOT_BOOL, FALSE)

	if (bPackSlot)
	{
		if (!pItem->RequiresPackSlot())
			return FALSE;
		
		int capacity = GetContainersCapacity();

		if (capacity >= 0)
		{
			if (m_Packs.size() < capacity)
				return TRUE;

			for (auto container : m_Packs)
			{
				if (container == pItem)
					return TRUE;
			}

			return FALSE;
		}
		else
		{
			if (InqBoolQuality(AI_ACCEPT_EVERYTHING_BOOL, FALSE))
				return TRUE;

			// check emote item acceptance here

			return FALSE;
		}
	}
	else
	{
		if (pItem->RequiresPackSlot())
			return FALSE;

		int capacity = GetItemsCapacity();

		if (capacity >= 0)
		{
			if (m_Items.size() < capacity)
				return TRUE;

			for (auto container : m_Items)
			{
				if (container == pItem)
					return TRUE;
			}

			return FALSE;
		}
		else
		{
			if (InqBoolQuality(AI_ACCEPT_EVERYTHING_BOOL, FALSE))
				return TRUE;

			// check emote item acceptance here
			if (m_Qualities._emote_table)
			{
				PackableList<EmoteSet> *emoteCategory = m_Qualities._emote_table->_emote_table.lookup(Give_EmoteCategory);

				if (emoteCategory)
				{
					for (auto &emoteSet : *emoteCategory)
					{
						if (emoteSet.classID == pItem->m_Qualities.id)
						{
							return TRUE;
						}
					}
				}
			}

			return FALSE;
		}
	}
}

void CContainerWeenie::Container_DeleteItem(DWORD item_id)
{
	CWeenieObject *item = FindContainedItem(item_id);
	if (!item)
		return;

	bool bWielded = item->IsWielded() ? true : false;

	// take it out of whatever slot it is in
	ReleaseContainedItemRecursive(item);

	item->SetWeenieContainer(0);
	item->SetWielderID(0);
	item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);
	
	if (bWielded && item->AsClothing())
	{
		UpdateModel();
	}

	DWORD RemoveObject[3];
	RemoveObject[0] = 0xF747;
	RemoveObject[1] = item->GetID();
	RemoveObject[2] = item->_instance_timestamp;
	g_pWorld->BroadcastPVS(this, RemoveObject, sizeof(RemoveObject));

	g_pWorld->RemoveEntity(item);
}

DWORD CContainerWeenie::Container_InsertInventoryItem(DWORD dwCell, CWeenieObject *item, DWORD slot)
{
	// You should check if the inventory is full before calling this.
	if (!item->RequiresPackSlot())
	{
		if (slot > (DWORD) m_Items.size())
			slot = (DWORD) m_Items.size();

		m_Items.insert(m_Items.begin() + slot, item);
	}
	else
	{
		if (slot > (DWORD) m_Packs.size())
			slot = (DWORD) m_Packs.size();

		m_Packs.insert(m_Packs.begin() + slot, item);
	}

	if (dwCell && m_bWorldIsAware)
	{
		item->_position_timestamp++;

		BinaryWriter Blah;
		Blah.Write<DWORD>(0xF74A);
		Blah.Write<DWORD>(item->GetID());
		Blah.Write<WORD>(item->_instance_timestamp);
		Blah.Write<WORD>(item->_position_timestamp);
		g_pWorld->BroadcastPVS(dwCell, Blah.GetData(), Blah.GetSize());
	}

	item->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
	item->unset_parent();
	item->leave_world();

	RecalculateEncumbrance();

	return slot;
}

bool CContainerWeenie::Container_TryStoreItemNow(DWORD item_id, DWORD container_id, DWORD slot, bool bSendEvent)
{
	CWeenieObject *pEntity = g_pWorld->FindWithinPVS(this, item_id);

	if (!pEntity || pEntity->IsStuck())
		return false;

	return Container_TryStoreItemNow((CWeenieObject *)pEntity, container_id, slot, bSendEvent);
}

bool CContainerWeenie::Container_TryStoreItemNow(CWeenieObject *item, DWORD container_id, DWORD slot, bool bSendEvent)
{
	// Scenarios to consider:
	// 1. Item being stored is equipped.
	// 2. Item being stored is on the GROUND!
	// 3. Item being stored is already in the inventory.
	// 4. Item being stored is in an external container (chest) or being moved to an external container (chest)

	CContainerWeenie *container = FindContainer(container_id);
	if (!container)
		return false;

	if (!item->CanPickup())
		return false;
	if (!container->Container_CanStore(item))
		return false;

	bool bWasWielded = item->IsWielded() ? true : false;

	DWORD dwCell = GetLandcell();

	if (!item->HasOwner())
	{
		if (CWeenieObject *generator = g_pWorld->FindObject(item->InqIIDQuality(GENERATOR_IID, 0)))
		{
			generator->NotifyGeneratedPickedUp(item->GetID());
		}
	}

	// Take it out of whatever slot it's in.
	item->ReleaseFromAnyWeenieParent(false, true);
	item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);

	/*
	if (item->InqIntQuality(CURRENT_WIELDED_LOCATION_INT, 0) != 0)
	{
		item->m_Qualities.SetInt(CURRENT_WIELDED_LOCATION_INT, 0);
		item->NotifyIntStatUpdated(CURRENT_WIELDED_LOCATION_INT);
	}
	*/

	item->SetWeenieContainer(container->GetID());
	item->ReleaseFromBlock();

	// The container will auto-correct this slot into a valid range.
	slot = container->Container_InsertInventoryItem(dwCell, item, slot);
	
	EmitSound(Sound_PickUpItem, 1.0f);

	if (bSendEvent)
	{
		if (item->AsContainer())
		{
			item->AsContainer()->MakeAwareViewContent(this);
		}

		SendNetMessage(InventoryMove(item->GetID(), container_id, slot, item->RequiresPackSlot() ? 1 : 0 ), PRIVATE_MSG, TRUE);
	}

	if (container->GetWorldTopLevelOwner() != this)
	{
		RecalculateEncumbrance();
	}

	if (item->GetItemType() & (TYPE_ARMOR | TYPE_CLOTHING))
	{
		UpdateModel();
	}

	if (bWasWielded && get_minterp()->InqStyle() != Motion_NonCombat)
	{
		AdjustToNewCombatMode();
	}

	item->m_Qualities.RemoveFloat(TIME_TO_ROT_FLOAT);
	item->_timeToRot = -1.0;
	item->_beganRot = false;

	return true;
}

bool CContainerWeenie::Container_TryDropItemNow(DWORD item_id)
{
	// Scenarios to consider:
	// 1. Item being dropped is equipped.
	// 3. Item being dropped is in the inventory.

	// Find the item.
	CWeenieObject *item = FindContainedItem(item_id);
	if (!item)
		return false;

	BOOL bWasWielded = item->IsWielded();

	DWORD dwCell = GetLandcell();

	// Take it out of whatever slot it's in.
	ReleaseContainedItemRecursive(item);

	item->SetWeenieContainer(0);
	item->SetWielderID(0);
	item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);

	EmitSound(Sound_DropItem, 1.0f);
	SendNetMessage(InventoryDrop(item_id), PRIVATE_MSG, TRUE);

	item->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
	item->unset_parent();
	item->enter_world(&m_Position);

	item->SetPlacementFrame(0x65, FALSE);

	//if (!pItem->HasAnims())
	//	pItem->SetPlacementFrame(pItem->CanPickup() ? 0x65 : 0, 1);

	if (item->AsClothing())
	{
		UpdateModel();
	}

	g_pWorld->InsertEntity(item);
	item->Movement_Teleport(GetPosition());

	item->_timeToRot = Timer::cur_time + 300.0;
	item->_beganRot = false;
	item->m_Qualities.SetFloat(TIME_TO_ROT_FLOAT, item->_timeToRot);

	RecalculateEncumbrance();

	if (bWasWielded && get_minterp()->InqStyle() != Motion_NonCombat)
	{
		AdjustToNewCombatMode();
	}

	return true;
}

DWORD CContainerWeenie::ReceiveInventoryItem(CWeenieObject *source, CWeenieObject *item, DWORD desired_slot)
{
	// by default, if we receive things just delete them... creatures can override this
	g_pWorld->RemoveEntity(item);
	return 0;
}

int CContainerWeenie::TryGiveObject(CContainerWeenie *target_container, CWeenieObject *source_object_weenie, DWORD amount)
{
	if (amount <= 0 || amount >= 100000)
		return WERROR_GIVE_NOT_ALLOWED;

	// for now we won't support giving items that are currently equipped
	if (source_object_weenie->IsEquipped() || source_object_weenie->parent)
		return WERROR_GIVE_NOT_ALLOWED;

	if (!target_container->Container_CanStore(source_object_weenie))
		return WERROR_GIVE_NOT_ALLOWED;

	CWeenieObject *target_top_level = target_container->GetWorldTopLevelOwner();

	if (target_top_level != source_object_weenie->GetWorldTopLevelOwner() && target_top_level && target_top_level->IsExecutingEmote())
	{
		SendText(csprintf("%s is busy.", target_top_level->GetName().c_str()), LTT_DEFAULT);
		return WERROR_GIVE_NOT_ALLOWED;
	}

	if (CPlayerWeenie *player = target_container->AsPlayer())
	{
		if (!(player->GetCharacterOptions() & AllowGive_CharacterOption))
			return WERROR_GIVE_NOT_ALLOWED;
	}

	int stackSize = source_object_weenie->InqIntQuality(STACK_SIZE_INT, 1, TRUE);
	if (stackSize < amount)
	{
		return WERROR_GIVE_NOT_ALLOWED;
	}

	CWeenieObject *object_given_weenie;
	if (stackSize > amount)
	{
		// partial stack, make a new object
		object_given_weenie = g_pWeenieFactory->CloneWeenie(source_object_weenie);
		object_given_weenie->SetID(g_pWorld->GenerateGUID(eDynamicGUID));

		if (!g_pWorld->CreateEntity(object_given_weenie, false))
		{
			return WERROR_GIVE_NOT_ALLOWED;
		}

		source_object_weenie->SetStackSize(stackSize - amount);
		object_given_weenie->SetStackSize(amount);

		BinaryWriter sourceSetStackSize;
		sourceSetStackSize.Write<DWORD>(0x197);
		sourceSetStackSize.Write<BYTE>(source_object_weenie->GetNextStatTimestamp(Int_StatType, STACK_SIZE_INT)); // FIXME TODO not sure sequence should be from item or the player
		sourceSetStackSize.Write<DWORD>(source_object_weenie->GetID());
		sourceSetStackSize.Write<DWORD>(source_object_weenie->InqIntQuality(STACK_SIZE_INT, 1));
		sourceSetStackSize.Write<DWORD>(source_object_weenie->InqIntQuality(VALUE_INT, 0));
		SendNetMessage(&sourceSetStackSize, PRIVATE_MSG, FALSE, FALSE);
	}
	else
	{
		object_given_weenie = source_object_weenie;
		source_object_weenie->ReleaseFromAnyWeenieParent();
	}

	target_container->EmitSound(Sound_ReceiveItem, 1.0f);

	object_given_weenie->m_Qualities.SetInstanceID(CONTAINER_IID, target_container->GetID());
	object_given_weenie->_cachedHasOwner = true;

	CWeenieObject *topLevelOwnerObj = object_given_weenie->GetWorldTopLevelOwner();
	assert(topLevelOwnerObj);
	assert(topLevelOwnerObj->AsContainer());

	if (topLevelOwnerObj)
	{
		if (CContainerWeenie *topLevelOwner = topLevelOwnerObj->AsContainer())
		{
			if (object_given_weenie == source_object_weenie)
			{
				SendNetMessage(InventoryMove(source_object_weenie->GetID(), target_container->GetID(), 0, 0), PRIVATE_MSG, TRUE);
			}

			if (amount > 1)
			{
				SendText(csprintf("You give %s %d %s.", topLevelOwner->GetName().c_str(), amount, object_given_weenie->GetPluralName().c_str()), LTT_DEFAULT);
				topLevelOwner->SendText(csprintf("%s gives you %d %s.", GetName().c_str(), amount, object_given_weenie->GetPluralName().c_str()), LTT_DEFAULT);
			}
			else
			{
				SendText(csprintf("You give %s %s.", topLevelOwner->GetName().c_str(), object_given_weenie->GetName().c_str()), LTT_DEFAULT);
				topLevelOwner->SendText(csprintf("%s gives you %s.", GetName().c_str(), object_given_weenie->GetName().c_str()), LTT_DEFAULT);
			}

			topLevelOwner->MakeAware(object_given_weenie, true);

			if (object_given_weenie->AsContainer())
			{
				object_given_weenie->AsContainer()->MakeAwareViewContent(topLevelOwner);
			}

			topLevelOwner->ReceiveInventoryItem(this, object_given_weenie, 0);
			topLevelOwner->DebugValidate();
		}
	}

	DebugValidate();

	RecalculateEncumbrance();

	return WERROR_NONE;
}

int CContainerWeenie::TryGiveObject(DWORD target_id, DWORD object_id, DWORD amount)
{
	CWeenieObject *object_weenie = FindContainedItem(object_id);
	CWeenieObject *target_weenie = g_pWorld->FindObject(target_id);

	if (!object_weenie || !target_weenie)
		return WERROR_OBJECT_GONE;

	if (target_weenie->IsContained())
		return WERROR_OBJECT_GONE;

	if (object_weenie == target_weenie)
		return WERROR_GIVE_NOT_ALLOWED;

	if (!target_weenie->AsContainer())
		return WERROR_GIVE_NOT_ALLOWED;

	if (object_weenie->IsAttunedOrContainsAttuned())
	{
		if (target_weenie->AsPlayer())
		{
			return WERROR_ATTUNED_ITEM;
		}
	}

	if (DistanceTo(target_weenie, true) > 1.0)
	{
		return WERROR_TOO_FAR;
	}

	return TryGiveObject(target_weenie->AsContainer(), object_weenie, amount);
}

CWeenieObject *CContainerWeenie::FindContained(DWORD object_id)
{
	return FindContainedItem(object_id);
}

void CContainerWeenie::InitPhysicsObj()
{
	CWeenieObject::InitPhysicsObj();

	if (!_phys_obj)
		return;
	
	for (auto item : m_Wielded)
	{
#ifdef _DEBUG
		assert(item->GetWielderID() == GetID());
#endif

		int parentLocation = item->InqIntQuality(PARENT_LOCATION_INT, PARENT_ENUM::PARENT_NONE);
		if (parentLocation != PARENT_ENUM::PARENT_NONE)
		{
			item->set_parent(this, parentLocation);

			int placement = item->InqIntQuality(PLACEMENT_POSITION_INT, 0);
			assert(placement);

			item->SetPlacementFrame(placement, FALSE);
		}
	}

#ifdef _DEBUG
	for (auto item : m_Items)
	{
		assert(item->InqIntQuality(PARENT_LOCATION_INT, PARENT_ENUM::PARENT_NONE) == PARENT_ENUM::PARENT_NONE);
	}

	for (auto pack : m_Packs)
	{
		assert(pack->InqIntQuality(PARENT_LOCATION_INT, PARENT_ENUM::PARENT_NONE) == PARENT_ENUM::PARENT_NONE);
	}
#endif
}

void CContainerWeenie::SaveEx(class CWeenieSave &save)
{
	CWeenieObject::SaveEx(save);

	for (auto item : m_Wielded)
	{
		save._equipment.push_back(item->GetID());
		item->Save();
	}

	for (auto item : m_Items)
	{
		save._inventory.push_back(item->GetID());
		item->Save();
	}

	for (auto item : m_Packs)
	{
		save._packs.push_back(item->GetID());
		item->Save();
	}
}

void CContainerWeenie::LoadEx(class CWeenieSave &save)
{
	CWeenieObject::LoadEx(save);

	for (auto item : save._equipment)
	{
		CWeenieObject *weenie = CWeenieObject::Load(item);

		if (weenie)
		{
			if (weenie->RequiresPackSlot())
			{
				delete weenie;
				continue;
			}

			assert(weenie->IsWielded());
			assert(!weenie->IsContained());

			// make sure it has the right settings (shouldn't be necessary)
			weenie->SetWielderID(GetID());
			weenie->SetWeenieContainer(0);
			weenie->m_Qualities.RemovePosition(INSTANTIATION_POSITION);
			weenie->m_Qualities.RemovePosition(LOCATION_POSITION);

			if (g_pWorld->CreateEntity(weenie, false))
			{
				m_Wielded.push_back(weenie);

				if (int combatUse = weenie->InqIntQuality(COMBAT_USE_INT, 0, TRUE))
					SetWieldedCombat(weenie, (COMBAT_USE)combatUse);

				assert(weenie->IsWielded());
				assert(!weenie->IsContained());
			}
			else
			{
				// remove any enchantments associated with this item that we failed to wield...
				if (m_Qualities._enchantment_reg)
				{
					PackableListWithJson<DWORD> spells_to_remove;

					if (m_Qualities._enchantment_reg->_add_list)
					{
						for (const auto &entry : *m_Qualities._enchantment_reg->_add_list)
						{
							if (entry._caster == item)
							{
								spells_to_remove.push_back(entry._id);
							}
						}
					}

					if (m_Qualities._enchantment_reg->_mult_list)
					{
						for (const auto &entry : *m_Qualities._enchantment_reg->_mult_list)
						{
							if (entry._caster == item)
							{
								spells_to_remove.push_back(entry._id);
							}
						}
					}

					if (m_Qualities._enchantment_reg->_cooldown_list)
					{
						for (const auto &entry : *m_Qualities._enchantment_reg->_cooldown_list)
						{
							if (entry._caster == item)
							{
								spells_to_remove.push_back(entry._id);
							}
						}
					}

					if (!spells_to_remove.empty())
					{
						m_Qualities._enchantment_reg->RemoveEnchantments(&spells_to_remove);
					}
				}
			}
		}
	}

	for (auto item : save._inventory)
	{
		CWeenieObject *weenie = CWeenieObject::Load(item);

		if (weenie)
		{
			DWORD correct_container_iid = weenie->m_Qualities.GetIID(CONTAINER_IID, 0);

			if (weenie->RequiresPackSlot() || (correct_container_iid && correct_container_iid != GetID()))
			{
				delete weenie;
				continue;
			}

			assert(!weenie->IsWielded());
			assert(weenie->IsContained());
			assert(!weenie->InqIntQuality(PARENT_LOCATION_INT, 0));

			// make sure it has the right settings (shouldn't be necessary)
			weenie->SetWielderID(0);
			weenie->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
			weenie->SetWeenieContainer(GetID());
			weenie->m_Qualities.RemovePosition(INSTANTIATION_POSITION);
			weenie->m_Qualities.RemovePosition(LOCATION_POSITION);

			if (g_pWorld->CreateEntity(weenie, false))
			{
				m_Items.push_back(weenie);

				assert(!weenie->IsWielded());
				assert(weenie->IsContained());
			}
		}
	}

	for (auto item : save._packs)
	{
		CWeenieObject *weenie = CWeenieObject::Load(item);

		if (weenie)
		{
			DWORD correct_container_iid = weenie->m_Qualities.GetIID(CONTAINER_IID, 0);

			if (!weenie->RequiresPackSlot() || (correct_container_iid && correct_container_iid != GetID()))
			{
				delete weenie;
				continue;
			}

			assert(!weenie->IsWielded());
			assert(weenie->IsContained());
			assert(!weenie->InqIntQuality(PARENT_LOCATION_INT, 0));

			// make sure it has the right settings (shouldn't be necessary)
			weenie->SetWielderID(0);
			weenie->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
			weenie->SetWeenieContainer(GetID());
			weenie->m_Qualities.RemovePosition(INSTANTIATION_POSITION);
			weenie->m_Qualities.RemovePosition(LOCATION_POSITION);

			if (g_pWorld->CreateEntity(weenie, false))
			{
				m_Packs.push_back(weenie);

				assert(!weenie->IsWielded());
				assert(weenie->IsContained());
			}
		}
	}
}

void CContainerWeenie::MakeAwareViewContent(CWeenieObject *other)
{
	BinaryWriter viewContent;
	viewContent.Write<DWORD>(0x196);
	viewContent.Write<DWORD>(GetID());

	PackableList<ContentProfile> inventoryList;
	for (auto item : m_Items)
	{
		ContentProfile prof;
		prof.m_iid = item->GetID();
		prof.m_uContainerProperties = 0;
		inventoryList.push_back(prof);
	}
	inventoryList.Pack(&viewContent);
	other->SendNetMessage(&viewContent, PRIVATE_MSG, TRUE, FALSE);

	for (auto item : m_Items)
	{
		other->MakeAware(item, true);
	}
}

bool CContainerWeenie::IsGroundContainer()
{
	if (HasOwner())
		return false;

	if (!InValidCell())
		return false;

	return true;
}

bool CContainerWeenie::IsInOpenRange(CWeenieObject *other)
{
	if (!IsGroundContainer())
		return false;

	if (DistanceTo(other, true) >= InqFloatQuality(USE_RADIUS_FLOAT, 1.0))
		return false;

	return true;
}

void CContainerWeenie::OnContainerOpened(CWeenieObject *other)
{
	if (other && other->_lastOpenedRemoteContainer && other->_lastOpenedRemoteContainer != GetID())
	{
		if (CWeenieObject *otherContainerObj = g_pWorld->FindObject(other->_lastOpenedRemoteContainer))
		{
			if (CContainerWeenie *otherContainer = otherContainerObj->AsContainer())
			{
				if (otherContainer->_openedBy == other->GetID())
					otherContainer->OnContainerClosed(other);
			}
		}
	}

	MakeAwareViewContent(other);
	_openedBy = other->GetID();
	other->_lastOpenedRemoteContainer = GetID();

	if (_nextReset < 0)
	{
		double resetInterval;
		if (m_Qualities.InqFloat(RESET_INTERVAL_FLOAT, resetInterval))
		{
			_nextReset = Timer::cur_time + resetInterval;
		}
	}
}

void CContainerWeenie::OnContainerClosed(CWeenieObject *other)
{
	if (other)
	{
		BinaryWriter closeContent;
		closeContent.Write<DWORD>(0x52);
		closeContent.Write<DWORD>(GetID());
		other->SendNetMessage(&closeContent, PRIVATE_MSG, TRUE, FALSE);
	}

	_openedBy = 0;
}

void CContainerWeenie::ResetToInitialState()
{
	if (_openedBy)
	{
		OnContainerClosed(g_pWorld->FindObject(_openedBy));
	}

	SetLocked(m_bInitiallyLocked ? TRUE : FALSE);

	while (!m_Wielded.empty())
		Container_DeleteItem((*m_Wielded.begin())->GetID());
	while (!m_Items.empty())
		Container_DeleteItem((*m_Items.begin())->GetID());
	while (!m_Packs.empty())
		Container_DeleteItem((*m_Packs.begin())->GetID());

	if (m_Qualities._generator_table && m_Qualities._generator_registry)
	{
		if (m_Qualities._generator_table)
		{
			for (auto &entry : m_Qualities._generator_table->_profile_list)
			{
				if (entry.whereCreate & Contain_RegenLocationType)
				{
					for (PackableHashTable<unsigned long, GeneratorRegistryNode>::iterator i = m_Qualities._generator_registry->_registry.begin(); i != m_Qualities._generator_registry->_registry.end();)
					{
						if (entry.slot == i->second.slot)
							i = m_Qualities._generator_registry->_registry.erase(i);
						else
							i++;
					}
				}
			}
		}
		else
		{
			m_Qualities._generator_registry->_registry.clear();
		}
	}

	InitCreateGenerator();
}

int CContainerWeenie::DoUseResponse(CWeenieObject *other)
{
	if (IsBusyOrInAction())
		return WERROR_NONE;

	if (!(GetItemType() & ITEM_TYPE::TYPE_CONTAINER))
		return WERROR_NONE;

	if (!IsGroundContainer())
		return WERROR_NONE;

	if (!IsInOpenRange(other))
		return WERROR_NONE;

	if (IsLocked())
	{
		EmitSound(Sound_OpenFailDueToLock, 1.0f);
		return WERROR_NONE;
	}

	int openError = CheckOpenContainer(other);

	if (openError)
		return WERROR_NONE;

	if (_openedBy)
	{
		if (_openedBy == other->GetID())
		{
			OnContainerClosed(other);
		}

		return WERROR_CHEST_ALREADY_OPEN;
	}

	OnContainerOpened(other);

	return WERROR_NONE;
}

void CContainerWeenie::InventoryTick()
{
	CWeenieObject::InventoryTick();

	for (auto wielded : m_Wielded)
	{
		wielded->InventoryTick();

#ifdef _DEBUG
		wielded->DebugValidate();
#endif
	}

	for (auto item : m_Items)
	{
		item->InventoryTick();

#ifdef _DEBUG
		item->DebugValidate();
#endif
	}

	for (auto pack : m_Packs)
	{
		pack->InventoryTick();

#ifdef _DEBUG
		pack->DebugValidate();
#endif
	}
}

void CContainerWeenie::Tick()
{
	CWeenieObject::Tick();

	if (_openedBy)
	{
		CheckToClose();
	}

	if (Timer::cur_time < _nextInventoryTick)
	{
		return;
	}

	for(auto wielded : m_Wielded)
	{
		wielded->InventoryTick();

#ifdef _DEBUG
		wielded->DebugValidate();
#endif
	}

	for (auto item : m_Items)
	{
		item->InventoryTick();

#ifdef _DEBUG
		item->DebugValidate();
#endif
	}

	for (auto pack : m_Packs)
	{
		pack->InventoryTick();

#ifdef _DEBUG
		pack->DebugValidate();
#endif
	}

	_nextInventoryTick = Timer::cur_time + Random::GenFloat(0.4, 0.6);
}

void CContainerWeenie::CheckToClose()
{
	if (_nextCheckToClose > Timer::cur_time)
	{
		return;
	}

	_nextCheckToClose = Timer::cur_time + 1.0;
	
	if (CWeenieObject *other = g_pWorld->FindObject(_openedBy))
	{
		if (other->IsDead() || !IsInOpenRange(other))
		{
			OnContainerClosed(other);
		}
	}
	else
	{
		OnContainerClosed(NULL);
	}
}

int CContainerWeenie::CheckOpenContainer(CWeenieObject *other)
{
	if (_openedBy)
	{
		if (_openedBy == other->GetID())
		{
			return WERROR_NONE;
		}
		
		return WERROR_CHEST_ALREADY_OPEN;
	}

	return WERROR_NONE;
}

void CContainerWeenie::HandleNoLongerViewing(CWeenieObject *other)
{
	if (!_openedBy || _openedBy != other->GetID())
		return;

	OnContainerClosed(other);
}

void CContainerWeenie::DebugValidate()
{
	CWeenieObject::DebugValidate();

#ifdef _DEBUG
	assert(!GetWielderID());
	
	for (auto wielded : m_Wielded)
	{
		assert(wielded->GetWielderID() == GetID());
		wielded->DebugValidate();
	}

	for (auto item : m_Items)
	{
		assert(item->GetContainerID() == GetID());
		item->DebugValidate();
	}

	for (auto pack : m_Packs)
	{
		assert(pack->GetContainerID() == GetID());
		pack->DebugValidate();
	}
#endif
}

void CContainerWeenie::RecalculateEncumbrance()
{
	int oldValue = InqIntQuality(ENCUMB_VAL_INT, 0);

	int newValue = 0;
	for (auto wielded : m_Wielded)
	{
		newValue += wielded->InqIntQuality(ENCUMB_VAL_INT, 0);
	}

	for (auto item : m_Items)
	{
		newValue += item->InqIntQuality(ENCUMB_VAL_INT, 0);
	}

	for (auto pack : m_Packs)
	{
		pack->RecalculateEncumbrance();
		newValue += pack->InqIntQuality(ENCUMB_VAL_INT, 0);
	}

	if (oldValue != newValue)
	{
		m_Qualities.SetInt(ENCUMB_VAL_INT, newValue);
		NotifyIntStatUpdated(ENCUMB_VAL_INT, true);
	}
}

bool CContainerWeenie::IsAttunedOrContainsAttuned()
{
	for (auto wielded : m_Wielded)
	{
		if (wielded->IsAttunedOrContainsAttuned())
			return true;
	}

	for (auto item : m_Items)
	{
		if (item->IsAttunedOrContainsAttuned())
			return true;
	}

	for (auto pack : m_Packs)
	{
		if (pack->IsAttunedOrContainsAttuned())
			return true;
	}

	return CWeenieObject::IsAttunedOrContainsAttuned();
}

bool CContainerWeenie::HasContainerContents()
{
	if (!m_Wielded.empty() || !m_Items.empty() || !m_Packs.empty())
		return true;

	return CWeenieObject::HasContainerContents();
}

void CContainerWeenie::AdjustToNewCombatMode()
{
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, READY_SLOT_LOC);

	COMBAT_MODE newCombatMode = COMBAT_MODE::UNDEF_COMBAT_MODE;

	for (auto item : wielded)
	{
		newCombatMode = item->GetEquippedCombatMode();

		if (newCombatMode != COMBAT_MODE::UNDEF_COMBAT_MODE)
		{
			break;
		}
	}

	if (newCombatMode == COMBAT_MODE::UNDEF_COMBAT_MODE)
	{
		newCombatMode = COMBAT_MODE::MELEE_COMBAT_MODE;
	}

	ChangeCombatMode(newCombatMode, false);
}