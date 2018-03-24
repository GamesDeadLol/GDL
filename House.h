
#pragma once

#include "WeenieObject.h"
#include "Portal.h"
#include "Chest.h"

class CHouseWeenie : public CWeenieObject
{
public:
	CHouseWeenie();

	virtual class CHouseWeenie *AsHouse() { return this; }

	virtual void EnsureLink(CWeenieObject *source) override;

	std::string GetHouseOwnerName();
	DWORD GetHouseOwner();
	DWORD GetHouseDID();
	int GetHouseType();

	std::set<DWORD> _hook_list;
};

class CSlumLordWeenie : public CWeenieObject
{
public:
	CSlumLordWeenie();

	virtual class CSlumLordWeenie *AsSlumLord() { return this; }

	CHouseWeenie *GetHouse();
	void GetHouseProfile(HouseProfile &prof);

	virtual int DoUseResponse(CWeenieObject *other) override;

	void BuyHouse(CPlayerWeenie *player, const PackableList<DWORD> &items);
	void RentHouse(CPlayerWeenie *player, const PackableList<DWORD> &items);
};

class CHookWeenie : public CContainerWeenie
{
public:
	CHookWeenie();

	virtual class CHookWeenie *AsHook() { return this; }

	class CHouseWeenie *GetHouse();
};

class CDeedWeenie : public CWeenieObject
{
public:
	CDeedWeenie();

	virtual class CDeedWeenie *AsDeed() { return this; }

	class CHouseWeenie *GetHouse();
};

class CBootSpotWeenie : public CWeenieObject
{
public:
	CBootSpotWeenie();

	virtual class CBootSpotWeenie *AsBootSpot() { return this; }

	class CHouseWeenie *GetHouse();
};

class CHousePortalWeenie : public CPortal
{
public:
	CHousePortalWeenie();

	virtual class CHousePortalWeenie *AsHousePortal() { return this; }
	virtual void ApplyQualityOverrides() override;

	class CHouseWeenie *GetHouse();

	virtual bool GetDestination(Position &position) override;
};

class CStorageWeenie : public CChestWeenie
{
public:
	CStorageWeenie();

	virtual class CStorageWeenie *AsStorage() { return this; }

	virtual void ApplyQualityOverrides() override;

	class CHouseWeenie *GetHouse();
};



