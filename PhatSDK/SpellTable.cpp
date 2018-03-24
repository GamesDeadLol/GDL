
#include "StdAfx.h"
#include "SpellTable.h"
#include "SpellComponentTable.h"

DEFINE_PACK(SpellFormula)
{
	UNFINISHED();
}

DEFINE_UNPACK(SpellFormula)
{
	for (int i = 0; i < 8; i++)
		_comps[i] = pReader->Read<DWORD>();

	return true;
}

BOOL SpellFormula::Complete()
{
	for (DWORD i = 0; i < 5; i++)
	{
		if (!_comps[i])
			return FALSE;
	}

	return TRUE;
}

ITEM_TYPE SpellFormula::GetTargetingType()
{
	int i = 5;
	for (; i < 8; i++)
	{
		if (!_comps[i])
			break;
	}
	return SpellComponentTable::GetTargetTypeFromComponentID(_comps[i - 1]);
}

DWORD SpellFormula::GetPowerLevelOfPowerComponent()
{
	return MagicSystem::DeterminePowerLevelOfComponent(_comps[0]);
}

DEFINE_PACK(Spell)
{
	UNFINISHED();
}

DEFINE_UNPACK(Spell)
{
	_spell_id = pReader->Read<DWORD>();
	return true;
}

double Spell::InqDuration()
{
	return -1.0;
}

Spell *Spell::BuildSpell(SpellType sp_type)
{
	switch (sp_type)
	{
	case SpellType::Enchantment_SpellType:
		return new EnchantmentSpell;
	case SpellType::Projectile_SpellType:
		return new ProjectileSpell;
	case SpellType::Boost_SpellType:
		return new BoostSpell;
	case SpellType::Transfer_SpellType:
		return new TransferSpell;
	case SpellType::PortalLink_SpellType:
		return new PortalLinkSpell;
	case SpellType::PortalRecall_SpellType:
		return new PortalRecallSpell;
	case SpellType::PortalSummon_SpellType:
		return new PortalSummonSpell;
	case SpellType::PortalSending_SpellType:
		return new PortalSendingSpell;
	case SpellType::Dispel_SpellType:
		return new DispelSpell;
	case SpellType::LifeProjectile_SpellType:
		return new ProjectileLifeSpell;
	case SpellType::EnchantmentProjectile_SpellType:
		return new ProjectileEnchantmentSpell;
	case SpellType::FellowBoost_SpellType:
		return new FellowshipBoostSpell;
	case SpellType::FellowEnchantment_SpellType:
		return new FellowshipEnchantmentSpell;
	case SpellType::FellowPortalSending_SpellType:
		return new FellowshipPortalSendingSpell;
	case SpellType::FellowDispel_SpellType:
		return new FellowshipDispelSpell;
	}

	return NULL;
}

void MetaSpell::Destroy()
{
	SafeDelete(_spell);
}

DEFINE_PACK(MetaSpell)
{
	UNFINISHED();
}

DEFINE_UNPACK(MetaSpell)
{
	_sp_type = (SpellType) pReader->Read<int>();

	if (_spell)
	{
		delete _spell;
	}

	_spell = Spell::BuildSpell(_sp_type);
	if (_spell)
	{
		_spell->UnPack(pReader);
	}

	return true;
}

void CSpellBase::Destroy()
{
	_meta_spell.Destroy();
}

DEFINE_PACK(CSpellBase)
{
	UNFINISHED();
}

DEFINE_UNPACK(CSpellBase)
{
	_name = pReader->ReadString();

#ifndef EMULATE_INFERRED_SPELL_DATA
	// these are obfuscated, swap low/high nibbles
	for (int i = 0; i < _name.size(); i++)
		_name[i] = (char)(BYTE)((BYTE)((BYTE)_name[i]<<4)| (BYTE)((BYTE)_name[i] >> 4));
#endif

	_desc = pReader->ReadString();

#ifndef EMULATE_INFERRED_SPELL_DATA
	// these are obfuscated, swap low/high nibbles
	for (int i = 0; i < _desc.size(); i++)
		_desc[i] = (char)(BYTE)((BYTE)((BYTE)_desc[i] << 4) | (BYTE)((BYTE)_desc[i] >> 4));
#endif

	_school = pReader->Read<DWORD>();
	_iconID = pReader->Read<DWORD>();
	_category = pReader->Read<DWORD>();
	_bitfield = pReader->Read<DWORD>();
	_base_mana = pReader->Read<int>();
	_base_range_constant = pReader->Read<float>();
	_base_range_mod = pReader->Read<float>();
	_power = pReader->Read<int>();
	_spell_economy_mod = pReader->Read<float>();
	_formula_version = pReader->Read<DWORD>();
	_component_loss = pReader->Read<float>();

	_meta_spell.UnPack(pReader);
	_formula.UnPack(pReader);

	_caster_effect = (PScriptType) pReader->Read<int>();
	_target_effect = (PScriptType)pReader->Read<int>();
	_fizzle_effect = (PScriptType)pReader->Read<int>();
	_recovery_interval = pReader->Read<double>();
	_recovery_amount = pReader->Read<float>();
	_display_order = pReader->Read<int>();
	_non_component_target_type = pReader->Read<DWORD>();
	_mana_mod = pReader->Read<int>();

	return true;
}

SpellFormula CSpellBase::InqSpellFormula() const
{
	SpellFormula formula = _formula;
	
	for (DWORD i = 0; i < SPELLFORMULA_MAX_COMPS; i++)
	{
		if (formula._comps[i])
			formula._comps[i] -= (PString::compute_hash(_name.c_str()) % 0x12107680) + (PString::compute_hash(_desc.c_str()) % 0xBEADCF45);
	}

	return formula;
}

STypeSkill CSpellBase::InqSkillForSpell() const
{
	switch (this->_school)
	{
	case 5:
		return STypeSkill::VOID_MAGIC_SKILL;

	case 1:
		return STypeSkill::WAR_MAGIC_SKILL;
	
	case 2:
		return STypeSkill::LIFE_MAGIC_SKILL;

	case 3:
		return STypeSkill::ITEM_ENCHANTMENT_SKILL;

	case 4:
		return STypeSkill::CREATURE_ENCHANTMENT_SKILL;
	}

	return STypeSkill::UNDEF_SKILL;
}

int CSpellBase::InqTargetType() const
{
	SpellFormula formula = InqSpellFormula();

	if (formula.Complete())
		return formula.GetTargetingType();

	return 0;
}

/*
DEFINE_PACK(SpellSetTierList)
{
	UNFINISHED();
}

DEFINE_UNPACK(SpellSetTierList)
{
}

DEFINE_PACK(SpellSet)
{
	UNFINISHED();
}

DEFINE_UNPACK(SpellSet)
{
}
*/

CSpellTable::CSpellTable()
{
}

CSpellTable::~CSpellTable()
{
	Destroy();
}

void CSpellTable::Destroy()
{
	for (auto &entry : _spellBaseHash)
	{
		entry.second.Destroy();
	}
}

DEFINE_DBOBJ(CSpellTable, SpellTables)
DEFINE_LEGACY_PACK_MIGRATOR(CSpellTable)

DEFINE_PACK(CSpellTable)
{
	pWriter->Write<DWORD>(id);
	_spellBaseHash.Pack(pWriter);
	// m_SpellSetHash....
}

DEFINE_UNPACK(CSpellTable) // type 0x0E00000E
{
	DWORD data_id = pReader->Read<DWORD>(); // id
	_spellBaseHash.UnPack(pReader);
	// m_SpellSetHash....

#if PHATSDK_IS_SERVER
	_categoryToResearchableSpellsMap.clear();

	for (auto &entry : _spellBaseHash)
	{
		int spellLevel = entry.second.InqSpellFormula().GetPowerLevelOfPowerComponent();
		if (spellLevel < 1 || spellLevel > 8)
			continue;

		if (spellLevel < 7)
		{
			if (entry.second._bitfield & NotResearchable_SpellIndex)
				continue;

			const char *suffix;
			switch (spellLevel)
			{
			case 1: suffix = " I"; break;
			case 2: suffix = " II"; break;
			case 3: suffix = " III"; break;
			case 4: suffix = " IV"; break;
			case 5: suffix = " V"; break;
			case 6: suffix = " VI"; break;
			default: continue;
			}

			std::string spellName = entry.second._name;
			const char *p = strstr(spellName.c_str(), suffix);
			if (!p)
				continue;

			int suffixPos = p - spellName.c_str();
			if (suffixPos != (spellName.length() - strlen(suffix)))
				continue;
		}

		if (auto entry1 = _categoryToResearchableSpellsMap.lookup(entry.second._category))
		{
			if (auto entry2 = entry1->lookup(entry.second._bitfield & (SelfTargeted_SpellIndex | FellowshipSpell_SpellIndex)))
			{
				if (auto entry3 = entry2->lookup(spellLevel))
				{
					// already have an entry
					continue;
				}
			}
		}

		_categoryToResearchableSpellsMap[entry.second._category][entry.second._bitfield & (SelfTargeted_SpellIndex|FellowshipSpell_SpellIndex)][spellLevel] = (SpellID) entry.first;
	}

#endif

	return true;
}

#if PHATSDK_IS_SERVER
DWORD CSpellTable::ChangeSpellToDifferentLevel(DWORD spell_id, DWORD spell_level)
{
	if (const CSpellBase *spell = GetSpellBase(spell_id))
	{
		if (auto categoryMap = _categoryToResearchableSpellsMap.lookup(spell->_category))
		{
			if (auto levelMap = categoryMap->lookup(spell->_bitfield & (SelfTargeted_SpellIndex | FellowshipSpell_SpellIndex)))
			{
				if (auto levelEntry = levelMap->lookup(spell_level))
				{
					return (DWORD) *levelEntry;
				}
			}
		}
	}

	return 0;
}
#endif

const CSpellBase *CSpellTable::GetSpellBase(DWORD spell_id)
{
	return _spellBaseHash.lookup(spell_id);
}
