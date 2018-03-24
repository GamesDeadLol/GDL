
#include "StdAfx.h"
#include "WeenieObject.h"
#include "Ammunition.h"
#include "Player.h"
#include "World.h"

CAmmunitionWeenie::CAmmunitionWeenie()
{
}

CAmmunitionWeenie::~CAmmunitionWeenie()
{
}

void CAmmunitionWeenie::ApplyQualityOverrides()
{
	CWeenieObject::ApplyQualityOverrides();
}

void CAmmunitionWeenie::MakeIntoMissile()
{
	int stackSize = 0;
	if (m_Qualities.InqInt(STACK_SIZE_INT, stackSize))
	{
		SetStackSize(1);
	}

	m_Qualities.SetInstanceID(WIELDER_IID, 0);
	m_Qualities.SetInstanceID(CONTAINER_IID, 0);
	_cachedHasOwner = false;

	m_Qualities.SetInt(CURRENT_WIELDED_LOCATION_INT, 0);
	m_Qualities.SetInt(PARENT_LOCATION_INT, 0);

	m_PhysicsState = INELASTIC_PS | GRAVITY_PS | PATHCLIPPED_PS | ALIGNPATH_PS | MISSILE_PS | REPORT_COLLISIONS_PS;
	SetInitialPhysicsState(m_PhysicsState);

	SetPlacementFrame(Placement::MissileFlight, FALSE);
}

void CAmmunitionWeenie::MakeIntoAmmo()
{
	set_state(INELASTIC_PS | GRAVITY_PS | IGNORE_COLLISIONS_PS | ETHEREAL_PS, m_bWorldIsAware ? TRUE : FALSE);
	SetInitialPhysicsState(m_PhysicsState);

	SetPlacementFrame(Placement::Resting, TRUE);

	_timeToRot = Timer::cur_time + 60.0;
	_beganRot = false;
	m_Qualities.SetFloat(TIME_TO_ROT_FLOAT, _timeToRot);
}

void CAmmunitionWeenie::PostSpawn()
{
	CWeenieObject::PostSpawn();

	if (m_PhysicsState & MISSILE_PS)
	{
		EmitEffect(PS_Launch, 0.0f);
	}
}

void CAmmunitionWeenie::HandleNonTargetCollision()
{
	Movement_UpdatePos();
	MakeIntoAmmo();
	_targetID = 0;

	if (CWeenieObject *source = g_pWorld->FindObject(_sourceID))
	{
		source->SendText("Your missile attack hit the environment.", LTT_DEFAULT);
		EmitSound(Sound_Collision, 1.0f);
	}

	// set the collision angle to be the heading of the object?
}

void CAmmunitionWeenie::HandleTargetCollision()
{
	MarkForDestroy();
}

BOOL CAmmunitionWeenie::DoCollision(const class EnvCollisionProfile &prof)
{
	HandleNonTargetCollision();
	return CWeenieObject::DoCollision(prof);
}

BOOL CAmmunitionWeenie::DoCollision(const class AtkCollisionProfile &prof)
{
	bool targetCollision = false;

	CWeenieObject *pHit = g_pWorld->FindWithinPVS(this, prof.id);
	if (pHit && (!_targetID || _targetID == pHit->GetID()) && (pHit->GetID() != _sourceID) && (pHit->GetID() != _launcherID))
	{
		CWeenieObject *pSource = g_pWorld->FindObject(_sourceID);

		if (!pHit->ImmuneToDamage(pSource))
		{
			targetCollision = true;
			
			DamageEventData dmgEvent;
			dmgEvent.source = pSource;
			dmgEvent.target = pHit;
			dmgEvent.damage_form = DF_MELEE;

			DWORD baseDamage;
			float variance;

			STypeSkill weaponSkill = STypeSkill::UNDEF_SKILL;

			double offenseMod = 1.0;

			CWeenieObject *weapon = g_pWorld->FindObject(_launcherID);
			if (weapon)
			{
				DWORD imbuedEffects = weapon->GetImbuedEffects();

				dmgEvent.weapon = weapon;
				baseDamage = GetAttackDamage() + pSource->GetAttackDamage();
				offenseMod = weapon->GetOffenseMod();
				variance = InqFloatQuality(DAMAGE_VARIANCE_FLOAT, 0.0f);
				weaponSkill = SkillTable::OldToNewSkill((STypeSkill)weapon->InqIntQuality(WEAPON_SKILL_INT, UNDEF_SKILL, TRUE));

				dmgEvent.damage_type = InqDamageType();
				dmgEvent.ignoreMagicResist = InqBoolQuality(IGNORE_MAGIC_RESIST_BOOL, FALSE) ? true : false;
				dmgEvent.ignoreMagicArmor = InqBoolQuality(IGNORE_MAGIC_ARMOR_BOOL, FALSE) ? true : false;
			
				DWORD skillLevel = 0;
				if (pSource->InqSkill(weaponSkill, skillLevel, FALSE))
				{
					skillLevel = (DWORD)(skillLevel * offenseMod);

					if (skillLevel >= 100)
					{
						baseDamage += (DWORD)((skillLevel - 100) * 0.11);
					}
				}

				dmgEvent.weaponSkill = skillLevel;

				bool bEvaded = false;

				DWORD missileDefense = 0;
				if (pHit->InqSkill(MISSILE_DEFENSE_SKILL, missileDefense, FALSE) && missileDefense > 0)
				{
					if (pHit->TryMissileEvade((DWORD)(skillLevel * (_attackPower + 0.5f))))
					{
						// send evasion message
						BinaryWriter attackerEvadeEvent;
						attackerEvadeEvent.Write<DWORD>(0x01B3);
						attackerEvadeEvent.WriteString(pHit->GetName());
						pSource->SendNetMessage(&attackerEvadeEvent, PRIVATE_MSG, TRUE, FALSE);

						BinaryWriter attackedEvadeEvent;
						attackedEvadeEvent.Write<DWORD>(0x01B4);
						attackedEvadeEvent.WriteString(pSource->GetName());
						pHit->SendNetMessage(&attackedEvadeEvent, PRIVATE_MSG, TRUE, FALSE);
						bEvaded = true;
					}
				}

				if (!bEvaded)
				{
					if (weaponSkill == BOW_SKILL || weaponSkill == CROSSBOW_SKILL)
					{
						DWORD attribMod = 0;
						if (pSource->m_Qualities.InqAttribute(COORDINATION_ATTRIBUTE, attribMod, FALSE))
						{
							baseDamage += attribMod / 20;
						}
					}

					double critMultiplier = weapon->GetCriticalMultiplier();
					double critChance = weapon->GetCriticalFrequency();

					if (imbuedEffects & CripplingBlow_ImbuedEffectType)
					{
						critMultiplier *= pSource->GetMeleeCripplingBlowMultiplier(skillLevel);

						if (critMultiplier > 7.0)
						{
							critMultiplier = 7.0;
						}
					}

					if (imbuedEffects & CriticalStrike_ImbuedEffectType)
					{
						critChance = pSource->GetMeleeCriticalStrikeFrequency(skillLevel);
					}

					if (imbuedEffects & IgnoreAllArmor_ImbuedEffectType)
					{
						dmgEvent.ignoreArmorEntirely = true;
					}

					//if (!pSource->AsPlayer())
					//	baseDamage *= 2;

					bool bCrit = (Random::GenFloat(0.0, 1.0) < critChance) ? true : false;

					double damageCalc;

					damageCalc = baseDamage;
					damageCalc *= 1.0f - Random::GenFloat(0.0f, variance);
					// damageCalc *= 0.5 + _attackPower;
					damageCalc *= weapon->InqFloatQuality(DAMAGE_MOD_FLOAT, 1.0);

					if (bCrit)
						damageCalc *= critMultiplier;

					damageCalc += 0.5f; // round it properly

					dmgEvent.wasCrit = bCrit;
					dmgEvent.outputDamage = dmgEvent.inputDamage = (DWORD)damageCalc;
					dmgEvent.hit_quadrant = (DAMAGE_QUADRANT)ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT; // TODO

					pSource->TryToDealDamage(dmgEvent);
				}
			}
		}
	}

	if (targetCollision)
		HandleTargetCollision();
	else
		HandleNonTargetCollision();

	return CWeenieObject::DoCollision(prof);
}

BOOL CAmmunitionWeenie::DoCollision(const class ObjCollisionProfile &prof)
{
	HandleNonTargetCollision();
	return CWeenieObject::DoCollision(prof);
}

void CAmmunitionWeenie::DoCollisionEnd(DWORD object_id)
{
	CWeenieObject::DoCollisionEnd(object_id);
}





