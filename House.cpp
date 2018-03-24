
#include "StdAfx.h"
#include "House.h"
#include "World.h"
#include "WeenieFactory.h"
#include "Player.h"
#include "InferredPortalData.h"

CHouseWeenie::CHouseWeenie()
{
}

void CHouseWeenie::EnsureLink(CWeenieObject *source)
{
	source->m_Qualities.SetInstanceID(HOUSE_IID, GetID());

	if (source->AsSlumLord())
	{
		m_Qualities.SetInstanceID(SLUMLORD_IID, source->GetID());
	}

	if (source->AsHook())
	{
		_hook_list.insert(source->GetID());
	}

	if (source->AsBootSpot())
	{
		m_Qualities.SetPosition(HOUSE_BOOT_POSITION, source->GetPosition());
	}



	// Deed_WeenieType
	// BootSpot_WeenieType
	// HousePortal_WeenieType
	// Storage_WeenieType
}

std::string CHouseWeenie::GetHouseOwnerName()
{
	return InqStringQuality(HOUSE_OWNER_NAME_STRING, "");
}

DWORD CHouseWeenie::GetHouseOwner()
{
	return InqIIDQuality(HOUSE_OWNER_IID, 0);
}

DWORD CHouseWeenie::GetHouseDID()
{
	return InqDIDQuality(HOUSEID_DID, 0);
}

int CHouseWeenie::GetHouseType()
{
	return InqIntQuality(HOUSE_TYPE_INT, 0);
}

CSlumLordWeenie::CSlumLordWeenie()
{
}

CHouseWeenie *CSlumLordWeenie::GetHouse()
{
	if (CWeenieObject *house = g_pWorld->FindObject(InqIIDQuality(HOUSE_IID, 0)))
	{
		return house->AsHouse();
	}

	return NULL;
}

void CSlumLordWeenie::GetHouseProfile(HouseProfile &prof)
{
	prof._slumlord = GetID();
	prof._maintenance_free = 0;
	prof._bitmask = Active_HouseBitmask;
	prof._min_level = InqIntQuality(MIN_LEVEL_INT, -1);
	prof._max_level = InqIntQuality(MAX_LEVEL_INT, -1);
	prof._min_alleg_rank = 0;

	if (m_Qualities._create_list)
	{
		for (auto &entry : *m_Qualities._create_list)
		{
			if (entry.regen_algorithm == HouseBuy_DestinationType)
			{
				HousePayment pay;
				pay.wcid = entry.wcid;
				pay.num = entry.amount;
				pay.paid = 0;

				CWeenieDefaults *weenieDef = g_pWeenieFactory->GetWeenieDefaults(entry.wcid);
				if (weenieDef)
				{
					weenieDef->m_Qualities.InqString(NAME_STRING, pay.name);
					weenieDef->m_Qualities.InqString(PLURAL_NAME_STRING, pay.pname);
				}
				prof._buy.push_back(pay);
			}
			else if (entry.regen_algorithm == HouseRent_DestinationType)
			{
				HousePayment pay;
				pay.wcid = entry.wcid;
				pay.num = entry.amount;
				pay.paid = 0;

				CWeenieDefaults *weenieDef = g_pWeenieFactory->GetWeenieDefaults(entry.wcid);
				if (weenieDef)
				{
					weenieDef->m_Qualities.InqString(NAME_STRING, pay.name);
					weenieDef->m_Qualities.InqString(PLURAL_NAME_STRING, pay.pname);
				}

				prof._rent.push_back(pay);
			}
		}
	}
	
	if (CHouseWeenie *house = GetHouse())
	{
		prof._name = house->GetHouseOwnerName();
		prof._owner = house->GetHouseOwner();
		prof._id = house->GetHouseDID();
		prof._type = house->GetHouseType();
	}
}

int CSlumLordWeenie::DoUseResponse(CWeenieObject *other)
{
	// 11711 is an example of a slumlord
	HouseProfile prof;
	GetHouseProfile(prof);
	
	BinaryWriter profMsg;
	profMsg.Write<DWORD>(0x21D);
	prof.Pack(&profMsg);
	other->SendNetMessage(&profMsg, PRIVATE_MSG, TRUE, FALSE);

	return WERROR_NONE;
}

void CSlumLordWeenie::BuyHouse(CPlayerWeenie *player, const PackableList<DWORD> &items)
{
	if (CHouseWeenie *house = GetHouse())
	{
		if (house->GetHouseOwner())
		{
			return;
		}

		for (auto &itemID : items)
		{
			CWeenieObject *item = g_pWorld->FindObject(itemID);

			if (!item || !player->FindContainedItem(itemID))
			{
				return;
			}

			/*
			item->ReleaseFromAnyWeenieParent();

			BinaryWriter removeInventoryObjectMessage;
			removeInventoryObjectMessage.Write<DWORD>(0x24);
			removeInventoryObjectMessage.Write<DWORD>(item->id);
			player->SendNetMessage(&removeInventoryObjectMessage, PRIVATE_MSG, FALSE, FALSE);

			item->MarkForDestroy();
			*/
		}

		player->SendText("Congratulations!  You now own this dwelling.", LTT_DEFAULT);
		player->m_Qualities.SetInt(HOUSE_PURCHASE_TIMESTAMP_INT, g_pPhatSDK->GetCurrTimeStamp());
		player->NotifyIntStatUpdated(HOUSE_PURCHASE_TIMESTAMP_INT);

		house->m_Qualities.SetInstanceID(HOUSE_OWNER_IID, player->GetID());
		house->NotifyIIDStatUpdated(HOUSE_OWNER_IID, false);

		house->m_Qualities.SetString(HOUSE_OWNER_NAME_STRING, player->GetName());

		for (auto hook_id : house->_hook_list)
		{
			CWeenieObject *hook = g_pWorld->FindObject(hook_id);

			if (!hook)
				continue;

			hook->m_Qualities.SetInstanceID(HOUSE_OWNER_IID, player->GetID());
			hook->NotifyIIDStatUpdated(HOUSE_OWNER_IID, false);

			hook->set_state(ETHEREAL_PS|IGNORE_COLLISIONS_PS, TRUE);

			hook->m_Qualities.SetBool(UI_HIDDEN_BOOL, FALSE);
			hook->NotifyBoolStatUpdated(UI_HIDDEN_BOOL, false);
		}

		DoForcedMotion(Motion_On);

		player->SendText("P.S. Housing is not done. This will not be saved, and features will not work yet.", LTT_DEFAULT);
	}
}


void CSlumLordWeenie::RentHouse(CPlayerWeenie *player, const PackableList<DWORD> &items)
{
	player->SendText("Paying maintenance is not necessary at this time.", LTT_DEFAULT);
}


CHookWeenie::CHookWeenie()
{
}

CHouseWeenie *CHookWeenie::GetHouse()
{
	if (CWeenieObject *house = g_pWorld->FindObject(InqIIDQuality(HOUSE_IID, 0)))
	{
		return house->AsHouse();
	}

	return NULL;
}

CDeedWeenie::CDeedWeenie()
{
}

CHouseWeenie *CDeedWeenie::GetHouse()
{
	if (CWeenieObject *house = g_pWorld->FindObject(InqIIDQuality(HOUSE_IID, 0)))
	{
		return house->AsHouse();
	}

	return NULL;
}

CBootSpotWeenie::CBootSpotWeenie()
{
}

CHouseWeenie *CBootSpotWeenie::GetHouse()
{
	if (CWeenieObject *house = g_pWorld->FindObject(InqIIDQuality(HOUSE_IID, 0)))
	{
		return house->AsHouse();
	}

	return NULL;
}

CHousePortalWeenie::CHousePortalWeenie()
{
}

void CHousePortalWeenie::ApplyQualityOverrides()
{
	SetRadarBlipColor(Portal_RadarBlipEnum);
}

CHouseWeenie *CHousePortalWeenie::GetHouse()
{
	if (CWeenieObject *house = g_pWorld->FindObject(InqIIDQuality(HOUSE_IID, 0)))
	{
		return house->AsHouse();
	}

	return NULL;
}

bool CHousePortalWeenie::GetDestination(Position &dest)
{
	if (CHouseWeenie *house = GetHouse())
	{
		if (Position *pos = g_pPortalDataEx->GetHousePortalDest(house->GetHouseDID(), m_Position.objcell_id))
		{
			dest = *pos;
			return true;
		}
	}

	return false;
}

CStorageWeenie::CStorageWeenie()
{
}

void CStorageWeenie::ApplyQualityOverrides()
{
}

CHouseWeenie *CStorageWeenie::GetHouse()
{
	if (CWeenieObject *house = g_pWorld->FindObject(InqIIDQuality(HOUSE_IID, 0)))
	{
		return house->AsHouse();
	}

	return NULL;
}

