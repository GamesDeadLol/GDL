
#include "StdAfx.h"
#include "TreasureFactory.h"
#include "WeenieObject.h"
#include "WeenieFactory.h"
#include "InferredPortalData.h"

CTreasureFactory::CTreasureFactory()
{
}

CTreasureFactory::~CTreasureFactory()
{
}

void CTreasureFactory::Initialize()
{
	std::ifstream fileStream("data\\json\\treasure.json");

	if (fileStream.is_open())
	{
		json jsonData;
		fileStream >> jsonData;
		fileStream.close();
		
		const json &tf = jsonData["treasure"];

		for (const auto &entry : tf)
		{
			int tier = entry["tier"];

			CLootTierFactory *lootFactory = GetLootTier(tier);

			if (lootFactory)
			{
				lootFactory->UnPackJson(entry);
			}
		}
	}
}

CLootTierFactory *CTreasureFactory::GetLootTier(int tier)
{
	if (tier < 1 || tier > 8)
		return NULL;

	return &_lootTiers[tier - 1];
}

CWeenieObject *CTreasureFactory::RollLootForTier(int tier)
{
	// winging the shit out of this

	float itemTypeRoll = Random::RollDice(0.0f, 1.0f);

	DWORD itemWcid = 0;

	if (itemTypeRoll < 0.05)
	{
		itemWcid = g_pPortalDataEx->_treasureTableData.RollHealer(tier);
	}
	else if (itemTypeRoll < 0.1)
	{
		itemWcid = g_pPortalDataEx->_treasureTableData.RollLockpick(tier);
	}
	else if (itemTypeRoll < 0.15)
	{
		itemWcid = g_pPortalDataEx->_treasureTableData.RollConsumable(tier);
	}
	else if (itemTypeRoll < 0.2)
	{
		itemWcid = g_pPortalDataEx->_treasureTableData.RollPea(tier);
	}
	else if (itemTypeRoll < 0.3)
	{
		if (itemWcid = g_pPortalDataEx->_treasureTableData.RollScroll(tier))
		{
			if (DWORD spell_id = g_pWeenieFactory->GetScrollSpellForWCID(itemWcid))
			{
				if (spell_id = CachedSpellTable->ChangeSpellToDifferentLevel(spell_id, max(1, min(6, Random::GenInt(tier - 1, tier)))))
				{
					itemWcid = g_pWeenieFactory->GetWCIDForScrollSpell(spell_id);
				}
			}
		}
	}
	else if (itemTypeRoll < 0.35)
	{
		itemWcid = g_pPortalDataEx->_treasureTableData.RollManaStone(tier);
	}
	else
	{
		itemWcid = g_pPortalDataEx->_treasureTableData.RollItem(tier);
	}

	CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(itemWcid);

	if (weenie)
	{
		int mutateDataInt = 0;
		if (weenie->m_Qualities.InqInt(TSYS_MUTATION_DATA_INT, mutateDataInt, TRUE, TRUE) && mutateDataInt)
		{
			int workmanship = g_pPortalDataEx->_treasureTableData.RollWorkmanship(tier);
			weenie->m_Qualities.SetInt(ITEM_WORKMANSHIP_INT, workmanship);

			DWORD mutateData = (DWORD)mutateDataInt;

			BYTE spellCode = (mutateData >> 24) & 0xFF;
			BYTE colorCode = (mutateData >> 16) & 0xFF;
			BYTE gemCode = (mutateData >> 8) & 0xFF;
			BYTE materialCode = (mutateData >> 0) & 0xFF;

			MaterialType baseMaterial = g_pPortalDataEx->_treasureTableData.RollBaseMaterialFromMaterialCode(materialCode, tier);
			MaterialType material = g_pPortalDataEx->_treasureTableData.RollMaterialFromBaseMaterial(baseMaterial, tier);
			int ptid = g_pPortalDataEx->_treasureTableData.RollPaletteTemplateIDFromMaterialAndColorCode(material, colorCode, tier);

			weenie->m_Qualities.SetInt(MATERIAL_TYPE_INT, material);
			weenie->m_Qualities.SetInt(VALUE_INT, (int)(weenie->InqIntQuality(VALUE_INT, 0, TRUE) * workmanship * (1.0f + g_pPortalDataEx->_treasureTableData.RollValueEnchantmentForMaterial(material, tier))));

			std::string materialName;
			if (MaterialTypeEnumMapper::MaterialTypeToString(material, materialName) && !materialName.empty())
			{
				if (materialName[materialName.size() - 1] != ' ')
					materialName += " ";

				weenie->m_Qualities.SetString(NAME_STRING, materialName + weenie->InqStringQuality(NAME_STRING, ""));
			}

			if (ptid)
			{
				weenie->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, ptid);
			}
			else
			{
				ptid = weenie->InqIntQuality(PALETTE_TEMPLATE_INT, 0, TRUE);
			}

			MutateIntMultiply(weenie, ARMOR_LEVEL_INT, 1.0f, 1.3f);
			MutateIntMultiply(weenie, ENCUMB_VAL_INT, 0.75f, 1.0f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_SLASH_FLOAT, 1.0f, 1.3f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_PIERCE_FLOAT, 1.0f, 1.3f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_BLUDGEON_FLOAT, 1.0f, 1.3f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_FIRE_FLOAT, 1.0f, 1.3f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_COLD_FLOAT, 1.0f, 1.3f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_ACID_FLOAT, 1.0f, 1.3f);
			MutateFloatMultiply(weenie, ARMOR_MOD_VS_ELECTRIC_FLOAT, 1.0f, 1.3f);
			
			if (ptid >= 1)
			{
				if (DWORD clothingBase = weenie->InqDIDQuality(CLOTHINGBASE_DID, 0))
				{
					if (ClothingTable *ct = ClothingTable::Get(clothingBase))
					{
						auto paletteEntry = ct->_paletteTemplatesHash.lookup(ptid);
						
						if (paletteEntry != NULL)
						{
							if (paletteEntry->numSubpalEffects >= 1)
								weenie->m_Qualities.SetFloat(SHADE_FLOAT, Random::GenFloat(0.0, 1.0));
							if (paletteEntry->numSubpalEffects >= 2)
								weenie->m_Qualities.SetFloat(SHADE2_FLOAT, Random::GenFloat(0.0, 1.0));
							if (paletteEntry->numSubpalEffects >= 3)
								weenie->m_Qualities.SetFloat(SHADE3_FLOAT, Random::GenFloat(0.0, 1.0));
							if (paletteEntry->numSubpalEffects >= 4)
								weenie->m_Qualities.SetFloat(SHADE4_FLOAT, Random::GenFloat(0.0, 1.0));
						}

						ClothingTable::Release(ct);
					}
				}
			}

			if (weenie->AsMissileLauncher())
			{
				MutateFloatMultiply(weenie, DAMAGE_MOD_FLOAT, 1.0f, 1.0 + (0.42f * (tier / 6.0f)));
			}

			int highestPowerSpell = 0;

			if (spellCode)
			{								
				int numSpells = 1;
				// if (Random::RollDice(0.0f, 1.0f) < 0.25f)
				// 	numSpells++;

				std::set<DWORD> baseSpells;
				for (DWORD i = 0; i < numSpells; i++)
				{
					DWORD spell = g_pPortalDataEx->_treasureTableData.RollSpell(spellCode, tier);

					if (spell && baseSpells.find(spell) == baseSpells.end())
					{
						baseSpells.insert(spell);

						int spellLevel = max(1, min(6, Random::GenInt(tier - 1, tier)));
						highestPowerSpell = max(highestPowerSpell, spellLevel);

						DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

						weenie->m_Qualities.AddSpell(alternateSpell ? alternateSpell : spell);
						weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
					}
				}
			}

			if (weenie->InqIntQuality(ARMOR_LEVEL_INT, 0, TRUE) > 0)
			{
				if (Random::RollDice(0.0f, 1.0f) < 0.5)
				{
					std::list<DWORD> spells = g_pPortalDataEx->_treasureTableData.RollArmorSpells(spellCode, tier);

					int spellNum = 0;
					for (auto spell : spells)
					{
						spellNum++;
						int spellLevel = max(1, min(6, Random::GenInt(tier - spellNum, tier)));
						highestPowerSpell = max(highestPowerSpell, spellLevel);

						DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

						weenie->m_Qualities.AddSpell(alternateSpell ? alternateSpell : spell);
						weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
					}
				}
			}

			if (weenie->AsCaster())
			{
				if (Random::RollDice(0.0f, 1.0f) < 0.5)
				{
					std::list<DWORD> spells = g_pPortalDataEx->_treasureTableData.RollCasterSpells(spellCode, tier);

					int spellNum = 0;
					for (auto spell : spells)
					{
						spellNum++;
						int spellLevel = max(1, min(6, Random::GenInt(tier - spellNum, tier)));
						highestPowerSpell = max(highestPowerSpell, spellLevel);

						DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

						weenie->m_Qualities.AddSpell(alternateSpell ? alternateSpell : spell);
						weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
					}
				}

				if (Random::RollDice(0.0f, 1.0f) < 0.5)
				{
					int numSpells = 1;
					if (Random::RollDice(0.0f, 1.0f) < 0.25f)
					{
						numSpells++;
					}

					std::set<DWORD> baseSpells;
					for (DWORD i = 0; i < numSpells; i++)
					{
						DWORD spell = g_pPortalDataEx->_treasureTableData.RollCasterBuffSpell(spellCode, tier);

						if (spell && baseSpells.find(spell) == baseSpells.end())
						{
							baseSpells.insert(spell);

							int spellLevel = max(1, min(6, Random::GenInt(tier - (i+1), tier)));
							highestPowerSpell = max(highestPowerSpell, spellLevel);

							DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

							weenie->m_Qualities.AddSpell(alternateSpell ? alternateSpell : spell);
							weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
						}
					}
				}

				if (Random::RollDice(0.0f, 1.0f) < 0.5)
				{
					DWORD spell = g_pPortalDataEx->_treasureTableData.RollCasterWarSpell(spellCode, tier);

					if (spell)
					{
						int spellLevel = max(1, min(6, Random::GenInt(tier - 1, tier)));
						highestPowerSpell = max(highestPowerSpell, spellLevel);

						DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

						weenie->m_Qualities.SetDataID(SPELL_DID, alternateSpell ? alternateSpell : spell);
						weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
					}
				}
			}

			if (weenie->AsMeleeWeapon())
			{
				if (Random::RollDice(0.0f, 1.0f) < 0.5)
				{										
					std::list<DWORD> spells = g_pPortalDataEx->_treasureTableData.RollMeleeWeaponSpells(spellCode, tier);

					int spellNum = 0;
					for (auto spell : spells)
					{
						spellNum++;
						int spellLevel = max(1, min(6, Random::GenInt(tier - spellNum, tier)));
						highestPowerSpell = max(highestPowerSpell, spellLevel);

						DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

						weenie->m_Qualities.AddSpell(alternateSpell ? alternateSpell : spell);
						weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
					}
				}
			}

			if (weenie->AsMissileLauncher())
			{
				if (Random::RollDice(0.0f, 1.0f) < 0.5)
				{
					std::list<DWORD> spells = g_pPortalDataEx->_treasureTableData.RollMissileWeaponSpells(spellCode, tier);

					int spellNum = 0;
					for (auto spell : spells)
					{
						spellNum++;
						int spellLevel = max(1, min(6, Random::GenInt(tier - spellNum, tier)));
						highestPowerSpell = max(highestPowerSpell, spellLevel);

						DWORD alternateSpell = CachedSpellTable->ChangeSpellToDifferentLevel(spell, spellLevel);

						weenie->m_Qualities.AddSpell(alternateSpell ? alternateSpell : spell);
						weenie->m_Qualities.SetInt(UI_EFFECTS_INT, weenie->InqIntQuality(UI_EFFECTS_INT, 0, TRUE) | UI_EFFECT_MAGICAL);
					}
				}
			}

			if (highestPowerSpell > 0)
			{
				// 40 + 30*(level-1) + -20 to 20
				int arcaneRequired = 50 + 40 * (highestPowerSpell - 1) + Random::GenInt(-20, 20);

				if (weenie->m_Qualities._spell_book)
				{
					arcaneRequired += max(0, ((int)weenie->m_Qualities._spell_book->_spellbook.size()) - 1) * 5;
				}

				/*
				if (Random::RollDice(0.0f, 1.0f) < 0.9f)
				{
					// add a race requirement
					weenie->m_Qualities.SetInt(WIELD_REQUIREMENTS_INT, 12);
					weenie->m_Qualities.SetInt(WIELD_DIFFICULTY_INT, Random::GenInt(1, 4)); // aluvian, gharundim, sho, viamontian
					arcaneRequired = (int)((float)arcaneRequired * 0.6f);
				}
				*/

				weenie->m_Qualities.SetInt(ITEM_DIFFICULTY_INT, arcaneRequired);
			}
		}
	}
	
	if (weenie)
	{
		weenie->m_Qualities.SetBool(GENERATED_TREASURE_ITEM_BOOL, TRUE);
	}

	return weenie;
}

void CTreasureFactory::MutateIntMultiply(CWeenieObject *weenie, STypeInt intStatType, float minV, float maxV)
{
	int val;
	if (weenie->m_Qualities.InqInt(intStatType, val, TRUE, TRUE))
	{
		val = (int)(val * Random::GenFloat(minV, maxV));
		weenie->m_Qualities.SetInt(intStatType, val);
	}
}

void CTreasureFactory::MutateFloatMultiply(CWeenieObject *weenie, STypeFloat floatStatType, float minV, float maxV)
{
	double val;
	if (weenie->m_Qualities.InqFloat(floatStatType, val, TRUE))
	{
		val = (double)(val * Random::GenFloat(minV, maxV));
		weenie->m_Qualities.SetFloat(floatStatType, val);
	}
}

CLootTierFactory::CLootTierFactory()
{
}

CLootTierFactory::~CLootTierFactory()
{
}

DEFINE_PACK_JSON(CLootTierFactory)
{
	_loot.PackJson(writer["loot"]);
}

DEFINE_UNPACK_JSON(CLootTierFactory)
{
	_loot.UnPackJson(reader["loot"]);
	return true;
}

CWeenieObject *CLootTierFactory::CreateRandomLoot()
{
	DWORD numLoot = (DWORD) _loot.size();
	if (numLoot <= 0)
		return NULL;

	DWORD lootIndex = Random::GenUInt(0, numLoot - 1);

	auto lootEntry = _loot.begin();
	std::advance(lootEntry, lootIndex);

	CWeenieObject *weenie;
	if (weenie = g_pWeenieFactory->CreateWeenieByClassID(lootEntry->wcid))
	{
		switch (weenie->m_Qualities.m_WeenieType)
		{
		case Generic_WeenieType:
			MutateGeneric(weenie);
			break;
		case Clothing_WeenieType:
			MutateClothing(weenie);
			break;
		}
	}

	return weenie;
}

void CLootTierFactory::MutateIntMultiply(CWeenieObject *weenie, STypeInt intStatType, float minV, float maxV)
{
	int val;
	if (weenie->m_Qualities.InqInt(intStatType, val, TRUE, TRUE))
	{
		val = (int)(val * Random::GenFloat(minV, maxV));
		weenie->m_Qualities.SetInt(intStatType, val);
	}
}

void CLootTierFactory::MutateFloatMultiply(CWeenieObject *weenie, STypeFloat floatStatType, float minV, float maxV)
{
	double val;
	if (weenie->m_Qualities.InqFloat(floatStatType, val, TRUE))
	{
		val = (double)(val * Random::GenFloat(minV, maxV));
		weenie->m_Qualities.SetFloat(floatStatType, val);
	}
}


void CLootTierFactory::MutateGeneric(CWeenieObject *weenie)
{
	MutateIntMultiply(weenie, ARMOR_LEVEL_INT, 1.0f, 1.3f);
	MutateIntMultiply(weenie, VALUE_INT, 1.0f, 3.0f);
	MutateIntMultiply(weenie, ENCUMB_VAL_INT, 0.8f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_SLASH_FLOAT, 1.0f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_PIERCE_FLOAT, 1.0f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_BLUDGEON_FLOAT, 1.0f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_FIRE_FLOAT, 1.0f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_COLD_FLOAT, 1.0f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_ACID_FLOAT, 1.0f, 1.2f);
	MutateFloatMultiply(weenie, ARMOR_MOD_VS_ELECTRIC_FLOAT, 1.0f, 1.2f);

	// random colors
	if (DWORD clothingBase = weenie->InqDIDQuality(CLOTHINGBASE_DID, 0))
	{
		if (ClothingTable *ct = ClothingTable::Get(clothingBase))
		{
			DWORD numEntries = (DWORD) ct->_paletteTemplatesHash.size();

			if (numEntries > 0)
			{
				DWORD paletteIndex = Random::GenUInt(0, numEntries - 1);

				auto paletteEntry = ct->_paletteTemplatesHash.begin();
				std::advance(paletteEntry, paletteIndex);

				weenie->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, paletteEntry->first);

				if (paletteEntry->second.numSubpalEffects >= 1)
					weenie->m_Qualities.SetFloat(SHADE_FLOAT, Random::GenFloat(0.0, 1.0));
				if (paletteEntry->second.numSubpalEffects >= 2)
					weenie->m_Qualities.SetFloat(SHADE2_FLOAT, Random::GenFloat(0.0, 1.0));
				if (paletteEntry->second.numSubpalEffects >= 3)
					weenie->m_Qualities.SetFloat(SHADE3_FLOAT, Random::GenFloat(0.0, 1.0));
				if (paletteEntry->second.numSubpalEffects >= 4)
					weenie->m_Qualities.SetFloat(SHADE4_FLOAT, Random::GenFloat(0.0, 1.0));
			}

			ClothingTable::Release(ct);
		}
	}
}

void CLootTierFactory::MutateClothing(CWeenieObject *weenie)
{
	MutateGeneric(weenie);
}

CLootEntry::CLootEntry()
{
}

DEFINE_PACK_JSON(CLootEntry)
{
	writer["wcid"] = wcid;
	// ptids.PackJson(writer["ptids"]);
	// writer["numShades"] = numShades;
	// writer["stackMin"] = stackMin;
	// writer["stackMax"] = stackMax;
}

DEFINE_UNPACK_JSON(CLootEntry)
{
	wcid = reader["wcid"];
	// ptids.UnPackJson(reader["ptids"]);
	// numShades = reader["numShades"];
	// stackMin = reader["stackMin"];
	// stackMax = reader["stackMax"];
	return true;
}


