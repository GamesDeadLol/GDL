
#pragma once

class CLootEntry : public PackableJson
{
public:
	CLootEntry();

	DECLARE_PACKABLE_JSON()
	
	DWORD wcid = 0;
	// PackableListWithJson<DWORD> ptids;
	// int numShades = 0;
	// int stackMin = 0;
	// int stackMax = 0;
	// PackableListWithJson<DWORD> spells;
};

class CLootTierFactory : public PackableJson
{
public:
	CLootTierFactory();
	virtual ~CLootTierFactory();

	DECLARE_PACKABLE_JSON()

	class CWeenieObject *CreateRandomLoot();
	
	void MutateIntMultiply(CWeenieObject *weenie, STypeInt intStatType, float minV, float maxV);
	void MutateFloatMultiply(CWeenieObject *weenie, STypeFloat floatStatType, float minV, float maxV);

	void MutateGeneric(CWeenieObject *weenie);
	void MutateClothing(CWeenieObject *weenie);

	PackableListWithJson<CLootEntry> _loot;
};

class CTreasureFactory
{
public:
	CTreasureFactory();
	virtual ~CTreasureFactory();

	void Initialize();

	CLootTierFactory *GetLootTier(int tier);

	CWeenieObject *RollLootForTier(int tier);

	void MutateIntMultiply(CWeenieObject *weenie, STypeInt intStatType, float minV, float maxV);
	void MutateFloatMultiply(CWeenieObject *weenie, STypeFloat floatStatType, float minV, float maxV);

	CLootTierFactory _lootTiers[8];
};