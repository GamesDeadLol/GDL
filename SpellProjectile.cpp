
#include "StdAfx.h"
#include "SpellProjectile.h"
#include "World.h"
#include "Monster.h"

const float MAX_SPELL_PROJECTILE_LIFETIME = 20.0f;

CSpellProjectile::CSpellProjectile(const SpellCastData &scd, DWORD target_id, unsigned int damage)
{
	m_CachedSpellCastData = scd;
	m_SourceID = scd.source_id;
	m_TargetID = target_id;
	m_Damage = damage;

	m_fSpawnTime = Timer::cur_time;
	m_fDestroyTime = FLT_MAX;

	m_fFriction = 1.0f;
	m_fElasticity = 0.0f;

	m_DefaultScript = 0x5A;
	m_DefaultScriptIntensity = 1.0f;

	m_fEffectMod = max(0, min(1.0, ((scd.power_level_of_power_component - 1.0) / 7.0)));

	SetItemType(ITEM_TYPE::TYPE_SELF);
	
	m_Qualities.SetBool(STUCK_BOOL, TRUE);
	m_Qualities.SetBool(ATTACKABLE_BOOL, TRUE);
	m_Qualities.SetBool(UI_HIDDEN_BOOL, TRUE);

	SetInitialPhysicsState(INELASTIC_PS | SCRIPTED_COLLISION_PS | REPORT_COLLISIONS_PS | MISSILE_PS | LIGHTING_ON_PS | PATHCLIPPED_PS | ALIGNPATH_PS);
}

CSpellProjectile::~CSpellProjectile()
{
}

void CSpellProjectile::Tick()
{
	if (!InValidCell() || (m_fSpawnTime + MAX_SPELL_PROJECTILE_LIFETIME) <= Timer::cur_time || (m_fDestroyTime <= Timer::cur_time))
	{
		MarkForDestroy();
	}
}

void CSpellProjectile::PostSpawn()
{
	CWeenieObject::PostSpawn();

	EmitEffect(PS_Launch, m_fEffectMod);
	m_fSpawnTime = Timer::cur_time;
}

void CSpellProjectile::HandleExplode()
{
	EmitEffect(PS_Explode, m_fEffectMod);

	// INELASTIC_PS | SCRIPTED_COLLISION_PS | REPORT_COLLISIONS_PS | MISSILE_PS | LIGHTING_ON_PS | PATHCLIPPED_PS | ALIGNPATH_PS
	set_state(/*0x128374*/
		INELASTIC_PS | SCRIPTED_COLLISION_PS | MISSILE_PS | PATHCLIPPED_PS | ALIGNPATH_PS | ETHEREAL_PS | IGNORE_COLLISIONS_PS | NODRAW_PS | CLOAKED_PS, TRUE);

	// no more REPORT_COLLISIONS_PS and no more LIGHTING_ON_PS
}

BOOL CSpellProjectile::DoCollision(const class EnvCollisionProfile &prof)
{
	Movement_UpdatePos();
	HandleExplode();
	m_fDestroyTime = Timer::cur_time + 1.0;

	return CWeenieObject::DoCollision(prof);
}

BOOL CSpellProjectile::DoCollision(const class AtkCollisionProfile &prof)
{
	CWeenieObject *pHit = g_pWorld->FindWithinPVS(this, prof.id);
	if (pHit && (!m_TargetID || m_TargetID == pHit->GetID()) && (pHit->GetID() != m_SourceID))
	{
		CWeenieObject *pSource = NULL;
		if (m_SourceID)
			pSource = g_pWorld->FindObject(m_SourceID);

		if (!pHit->ImmuneToDamage(pSource))
		{
			Movement_UpdatePos();
			// EmitEffect(5, 0.8f);
			HandleExplode();
			m_fDestroyTime = Timer::cur_time + 1.0;

			// try to resist
			bool bResisted = false;

			if (m_CachedSpellCastData.spell->_bitfield & Resistable_SpellIndex)
			{
				if (pSource != pHit)
				{
					if (pHit->TryMagicResist(m_CachedSpellCastData.current_skill))
					{
						pHit->EmitSound(Sound_ResistSpell, 1.0f, false);

						if (pSource)
						{
							pHit->EmitSound(Sound_ResistSpell, 1.0f, false);
							pHit->SendText(csprintf("You resist the spell cast by %s", pSource->GetName().c_str()), LTT_MAGIC);
							pSource->SendText(csprintf("%s resists your spell", pHit->GetName().c_str()), LTT_MAGIC);
							pHit->ResistedSpell(pSource);						
							pHit->ChanceExecuteEmoteSet(pSource->GetID(), ResistSpell_EmoteCategory);
						}
						bResisted = true;
					}
				}
			}

			if (!bResisted)
			{
				pHit->HandleAggro(pSource);							

				DamageEventData dmgEvent;
				dmgEvent.weapon = g_pWorld->FindObject(m_CachedSpellCastData.wand_id);
				dmgEvent.source = pSource;
				dmgEvent.target = pHit;
				dmgEvent.damage_form = DF_MAGIC;
				dmgEvent.spell_name = m_CachedSpellCastData.spell->_name;
				dmgEvent.damage_type = InqDamageType();
				dmgEvent.isProjectile = true;

				double damage = m_Damage;

				double critFrequency = (dmgEvent.weapon && dmgEvent.weapon->AsCaster() ?
					dmgEvent.weapon->InqFloatQuality(CRITICAL_FREQUENCY_FLOAT, 0.05, FALSE) : 0.05);

				if (critFrequency >= Random::RollDice(0.0, 1.0))
				{
					dmgEvent.wasCrit = true;
					damage *= 1.0 + (dmgEvent.weapon && dmgEvent.weapon->AsCaster() ?
						dmgEvent.weapon->InqFloatQuality(CRITICAL_MULTIPLIER_FLOAT, 0.25, FALSE) : 0.25);
				}

				dmgEvent.outputDamage = dmgEvent.inputDamage = (float)damage;
				pHit->TryToDealDamage(dmgEvent);
			}
		}
	}

	return CWeenieObject::DoCollision(prof);
}

BOOL CSpellProjectile::DoCollision(const class ObjCollisionProfile &prof)
{
	Movement_UpdatePos();
	// EmitEffect(5, 0.8f);
	HandleExplode();
	m_fDestroyTime = Timer::cur_time + 1.0;

	return CWeenieObject::DoCollision(prof);
}

void CSpellProjectile::DoCollisionEnd(DWORD object_id)
{
	CWeenieObject::DoCollisionEnd(object_id);
}



