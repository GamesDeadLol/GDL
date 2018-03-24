
#include "StdAfx.h"
#include "Vendor.h"
#include "WeenieFactory.h"
#include "World.h"
#include "Player.h"
#include "EmoteManager.h"

CVendorItem::CVendorItem()
{
}

CVendorItem::~CVendorItem()
{
	g_pWorld->EnsureRemoved(weenie);
	SafeDelete(weenie);
}

CVendor::CVendor()
{
	m_Qualities.m_WeenieType = Vendor_WeenieType;
}

CVendor::~CVendor()
{
	ResetItems();
}

void CVendor::ResetItems()
{
	for (auto item : m_Items)
	{
		delete item;
	}

	m_Items.clear();
}

void CVendor::AddVendorItem(DWORD wcid, int ptid, float shade, int amount)
{
	CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(wcid);

	if (!weenie)
		return;

	if (ptid)
		weenie->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, ptid);

	if (shade >= 0.0)
		weenie->m_Qualities.SetFloat(SHADE_FLOAT, shade);

	if (!g_pWorld->CreateEntity(weenie))
		return;

	CVendorItem *item = new CVendorItem();
	item->weenie = weenie;
	item->amount = amount;
	m_Items.push_back(item);
}

void CVendor::AddVendorItem(DWORD wcid, int amount)
{
	CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(wcid);
		
	if (!weenie)
		return;
	
	if (!g_pWorld->CreateEntity(weenie))
		return;

	CVendorItem *item = new CVendorItem();
	item->weenie = weenie;
	item->amount = amount;
	m_Items.push_back(item);
}

CVendorItem *CVendor::FindVendorItem(DWORD item_id)
{
	for (auto item : m_Items)
	{
		if (item->weenie->GetID() == item_id)
			return item;
	}

	return NULL;
}

int CVendor::TrySellItemsToPlayer(CPlayerWeenie *buyer, const std::list<ItemProfile *> &desiredItems)
{
	// TODO properly handle stackables (don't add stackables until that's done.)

	const DWORD MAX_COIN_PURCHASE = 2000000000; // // limit to purchases less than 2 billion pyreal

	// using WERROR_NO_OBJECT as a generic error, change later if it matters
	if (desiredItems.size() >= 100)
		return WERROR_NO_OBJECT;

	// check cost of items
	UINT64 totalCost = 0;
	DWORD totalSlotsRequired = 0;
	for (auto desiredItem : desiredItems)
	{
		CVendorItem *vendorItem = FindVendorItem(desiredItem->iid);
		if (!vendorItem)
			return WERROR_NO_OBJECT;
		if (vendorItem->amount >= 0 && vendorItem->amount < desiredItem->amount)
			return WERROR_NO_OBJECT;
		if (desiredItem->amount < 0 || desiredItem->amount >= 1000)
			return WERROR_NO_OBJECT;
		if (desiredItem->amount == 0)
			continue;

		double largeCheck = (double)(DWORD)(vendorItem->weenie->GetValue() * profile.sell_price) * desiredItem->amount;
		if (largeCheck >= 2000000000)
			return WERROR_NO_OBJECT; // limit to purchases less than 2 billion pyreal

		totalCost += (DWORD)(vendorItem->weenie->GetValue() * profile.sell_price) * desiredItem->amount;
		totalSlotsRequired += desiredItem->amount;

		if (totalCost >= 2000000000)
			return WERROR_NO_OBJECT; // purchase too large
	}
	
	if (buyer->GetCoin() < totalCost || totalCost >= MAX_COIN_PURCHASE)
		return WERROR_NO_OBJECT; // limit to purchases less than 2 billion pyreal

	if (buyer->Container_GetNumFreeMainPackSlots() < totalSlotsRequired)
		return WERROR_NO_OBJECT;

	buyer->SetCoin(buyer->GetCoin() - totalCost);
	buyer->NotifyIntStatUpdated(COIN_VALUE_INT);

	// clone the weenie
	for (auto desiredItem : desiredItems)
	{
		for (DWORD i = 0; i < desiredItem->amount; i++)
		{
			CWeenieObject *originalWeenie = FindVendorItem(desiredItem->iid)->weenie;
			CWeenieObject *weenie = g_pWeenieFactory->CloneWeenie(originalWeenie);

			weenie->SetID(g_pWorld->GenerateGUID(eDynamicGUID));

			if (buyer->Container_TryStoreItemNow(weenie, buyer->GetID(), 0, true))
			{
				if (!g_pWorld->CreateEntity(weenie))
					continue;

				buyer->MakeAware(weenie);
			}
			else
			{
				weenie->SetInitialPosition(buyer->m_Position);

				if (!g_pWorld->CreateEntity(weenie))
					continue;

				weenie->_timeToRot = Timer::cur_time + 300.0;
				weenie->_beganRot = false;
				weenie->m_Qualities.SetFloat(TIME_TO_ROT_FLOAT, weenie->_timeToRot);
			}
		}
	}

	DoVendorEmote(Buy_VendorTypeEmote, buyer->GetID());

	return WERROR_NONE;
}


int CVendor::TryBuyItemsFromPlayer(CPlayerWeenie *seller, const std::list<ItemProfile *> &desiredItems)
{
	// TODO properly handle stackables (don't add stackables until that's done.)

	const DWORD MAX_COIN_PURCHASE = 1000000000; // limit to purchases less than 2 billion pyreal
	const DWORD MAX_COIN_ALLOWED = 2000000000; // limit to purchases less than 2 billion pyreal

	// using WERROR_NO_OBJECT as a generic error, change later if it matters
	if (desiredItems.size() >= 100)
		return WERROR_NO_OBJECT;

	// check price of items
	UINT64 totalCost = 0;
	DWORD totalSlotsRequired = 0;
	for (auto desiredItem : desiredItems)
	{
		CWeenieObject *sellerItem = seller->FindContainedItem(desiredItem->iid);
		if (!sellerItem)
			return WERROR_NO_OBJECT;
		if (sellerItem->InqIntQuality(STACK_SIZE_INT, 1) < desiredItem->amount)
			return WERROR_NO_OBJECT;
		if (sellerItem->HasContainerContents()) // can't buy containers that have items in them
			return WERROR_NO_OBJECT;
		if (desiredItem->amount <= 0)
			continue;

		double largeCheck;

		if (sellerItem->InqIntQuality(STACK_SIZE_INT, 1) != desiredItem->amount)
			largeCheck = (double)(DWORD)(sellerItem->InqIntQuality(STACK_UNIT_VALUE_INT, 0) * profile.buy_price) * desiredItem->amount;
		else
			largeCheck = (double)(DWORD)(sellerItem->InqIntQuality(VALUE_INT, 0) * profile.buy_price);

		if (largeCheck >= 2000000000)
			return WERROR_NO_OBJECT; // limit to purchases less than 2 billion pyreal

		if (sellerItem->InqIntQuality(STACK_SIZE_INT, 1) != desiredItem->amount)
			totalCost += (DWORD)(sellerItem->InqIntQuality(STACK_UNIT_VALUE_INT, 0) * profile.buy_price) * desiredItem->amount;
		else
			totalCost += (DWORD)(sellerItem->InqIntQuality(VALUE_INT, 0) * profile.buy_price);

		if (totalCost >= MAX_COIN_PURCHASE)
			return WERROR_NO_OBJECT; // purchase too large
	}

	UINT64 newCoin = seller->GetCoin() + totalCost;

	if (newCoin > MAX_COIN_ALLOWED)
		return WERROR_NO_OBJECT; // user will have too much wealth

	seller->SetCoin((DWORD)newCoin);
	seller->NotifyIntStatUpdated(COIN_VALUE_INT);

	// take away the items purchased...

	for (auto desiredItem : desiredItems)
	{
		CWeenieObject *weenie = seller->FindContainedItem(desiredItem->iid);

		if (!weenie)
			continue;

		bool bShouldRemove = false;

		int stackSize = 0;
		if (weenie->m_Qualities.InqInt(STACK_SIZE_INT, stackSize))
		{
			if (stackSize <= desiredItem->amount)
			{
				bShouldRemove = true;
			}
			else
			{
				weenie->SetStackSize(stackSize - desiredItem->amount);
			}
		}
		else
		{
			if (desiredItem->amount >= 1)
			{
				// delete the item
				bShouldRemove = true;
			}
		}

		if (bShouldRemove)
		{
			weenie->ReleaseFromAnyWeenieParent();

			BinaryWriter removeInventoryObjectMessage;
			removeInventoryObjectMessage.Write<DWORD>(0x24);
			removeInventoryObjectMessage.Write<DWORD>(weenie->GetID());
			seller->SendNetMessage(&removeInventoryObjectMessage, PRIVATE_MSG, FALSE, FALSE);

			weenie->MarkForDestroy();
		}
	}

	DoVendorEmote(Sell_VendorTypeEmote, seller->GetID());

	return WERROR_NONE;
}

void CVendor::AddVendorItemByAllMatchingNames(const char *name)
{
	int index = 0;
	DWORD wcid;
	
	while (wcid = g_pWeenieFactory->GetWCIDByName(name, index++))
	{
		AddVendorItem(wcid, -1);
	}
}

void CVendor::GenerateItems()
{
	ResetItems();
}

void CVendor::GenerateAllItems()
{
	ResetItems();
}

void CVendor::ValidateItems()
{
}

void CVendor::PreSpawnCreate()
{
	CMonsterWeenie::PreSpawnCreate();

	profile.min_value = InqIntQuality(MERCHANDISE_MIN_VALUE_INT, 0, TRUE);
	profile.max_value = InqIntQuality(MERCHANDISE_MAX_VALUE_INT, 0, TRUE);
	profile.magic = InqBoolQuality(DEAL_MAGICAL_ITEMS_BOOL, FALSE);
	profile.item_types = InqIntQuality(MERCHANDISE_ITEM_TYPES_INT, 0, TRUE);

	if (m_Qualities._create_list)
	{
		for (auto i = m_Qualities._create_list->begin(); i != m_Qualities._create_list->end(); i++)
		{
			if (i->destination == DestinationType::Shop_DestinationType)
			{
				AddVendorItem(i->wcid, i->palette, i->shade, -1);
			}
		}
	}
}

void CVendor::SendVendorInventory(CWeenieObject *other)
{
	ValidateItems();

	BinaryWriter vendorInfo;
	vendorInfo.Write<DWORD>(0x62);
	vendorInfo.Write<DWORD>(GetID());
	profile.Pack(&vendorInfo);
	
	vendorInfo.Write<DWORD>(m_Items.size());
	for (auto item : m_Items)
	{
		ItemProfile itemProfile;
		itemProfile.amount = item->amount;
		itemProfile.iid = item->weenie->GetID(); // for now, use the WCID plus arbitrary number to reference this item
		itemProfile.pwd = PublicWeenieDesc::CreateFromQualities(&item->weenie->m_Qualities);
		itemProfile.pwd->_containerID = GetID();
		itemProfile.Pack(&vendorInfo);
	}

	other->SendNetMessage(&vendorInfo, PRIVATE_MSG, TRUE, FALSE);
}

void CVendor::DoVendorEmote(int type, DWORD target_id)
{
	if (m_Qualities._emote_table)
	{
		PackableList<EmoteSet> *emoteSetList = m_Qualities._emote_table->_emote_table.lookup(Vendor_EmoteCategory);

		if (emoteSetList)
		{
			double dice = Random::GenFloat(0.0, 1.0);

			for (auto &emoteSet : *emoteSetList)
			{
				if (emoteSet.vendorType != type)
					continue;

				if (dice < emoteSet.probability)
				{
					MakeEmoteManager()->ExecuteEmoteSet(emoteSet, target_id);

					break;
				}

				dice -= emoteSet.probability;
			}
		}
	}
}

int CVendor::DoUseResponse(CWeenieObject *player)
{
	if (IsCompletelyIdle())
	{
		MovementParameters params;
		TurnToObject(player->GetID(), &params);
	}

	SendVendorInventory(player);
	DoVendorEmote(Open_VendorTypeEmote, player->GetID());
	return WERROR_NONE;
}

void CAvatarVendor::PreSpawnCreate()
{
	CVendor::PreSpawnCreate();

	// ResetItems();

	for (DWORD i = 0; i < g_pWeenieFactory->GetNumAvatars(); i++)
	{
		AddVendorItem(g_pWeenieFactory->GetFirstAvatarWCID() + i, -1);
	}
}
