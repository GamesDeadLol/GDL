
#include "StdAfx.h"
#include "AttackManager.h"
#include "WeenieObject.h"
#include "World.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "Ammunition.h"

// TODO fix memory leak with attack data

const double DISTANCE_REQUIRED_FOR_MELEE_ATTACK = 1.0;
const double MAX_MELEE_ATTACK_CONE_ANGLE = 15.0;
const double MAX_MISSILE_ATTACK_CONE_ANGLE = 3.0;

CAttackEventData::CAttackEventData()
{
}

void CAttackEventData::Update()
{
	if (_attack_charge_time >= 0.0 && Timer::cur_time >= _attack_charge_time)
	{
		PostCharge();
	}

	CheckTimeout();
}

void CAttackEventData::Setup()
{
	_max_attack_distance = DISTANCE_REQUIRED_FOR_MELEE_ATTACK;
	_max_attack_angle = MAX_MELEE_ATTACK_CONE_ANGLE;
	_timeout = Timer::cur_time + 15.0;
}

void CAttackEventData::PostCharge()
{
	_attack_charge_time = -1.0;

	if (InAttackCone())
	{
		OnReadyToAttack();
	}
	else
	{
		MoveToAttack();
	}
}

void CAttackEventData::Begin()
{
	Setup();

	CWeenieObject *target = GetTarget();
	if (!target)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (target->HasOwner())
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!target->IsAttackable())
	{
		Cancel();
		return;
	}
	
	if (_attack_charge_time < 0.0 || Timer::cur_time >= _attack_charge_time)
	{
		PostCharge();
	}
}

void CAttackEventData::MoveToAttack()
{
	_move_to = true;
	
	MovementParameters params;
	params.can_walk = 0;
	params.can_run = 0;
	params.can_sidestep = 0;
	params.can_walk_backwards = 0;
	params.move_away = 1;
	params.can_charge = 1;
	params.fail_walk = 1;
	params.use_final_heading = 1;
	params.sticky = _use_sticky;

	params.min_distance = 0.1f; 
	params.distance_to_object = _max_attack_distance - 0.5f; // 0.5
	params.fail_distance = _fail_distance;
	params.speed = 1.5f;
	params.action_stamp = ++_weenie->m_wAnimSequence;
	_weenie->last_move_was_autonomous = false;
	_weenie->MoveToObject(_target_id, &params);
}

void CAttackEventData::CheckTimeout()
{
	if (Timer::cur_time > _timeout)
	{
		if (_move_to)
			Cancel(WERROR_MOVED_TOO_FAR);
		else
			Cancel(0);
	}
}

void CAttackEventData::Cancel(DWORD error)
{
	CancelMoveTo();

	_manager->OnAttackCancelled(error);
}

void CAttackEventData::CancelMoveTo()
{
	if (_move_to)
	{
		_weenie->cancel_moveto();
		_weenie->Animation_MoveToUpdate();

		_move_to = false;
	}
}

double CAttackEventData::DistanceToTarget()
{
	if (!_target_id || _target_id == _weenie->GetID())
		return 0.0;

	CWeenieObject *target = GetTarget();
	if (!target)
		return FLT_MAX;

	return _weenie->DistanceTo(target, true);
}

double CAttackEventData::HeadingToTarget()
{
	if (!_target_id || _target_id == _weenie->GetID())
		return 0.0;

	CWeenieObject *target = GetTarget();
	if (!target)
		return 0.0;

	return _weenie->HeadingDiffTo(target);
}

bool CAttackEventData::InAttackRange()
{
	CWeenieObject *target = GetTarget();
	if (!target || target->HasOwner())
		return true;

	if ((_max_attack_distance + F_EPSILON) < DistanceToTarget())
		return false;

	return true;
}

bool CAttackEventData::InAttackCone()
{
	CWeenieObject *target = GetTarget();
	if (!target || target->HasOwner())
		return true;

	if ((_max_attack_distance + F_EPSILON) < DistanceToTarget())
		return false;
	if ((_max_attack_angle + F_EPSILON) < HeadingToTarget())
		return false;

	return true;
}

CWeenieObject *CAttackEventData::GetTarget()
{
	return g_pWorld->FindObject(_target_id);
}

void CAttackEventData::HandleMoveToDone(DWORD error)
{
	_move_to = false;

	if (error)
	{
		Cancel(error);
		return;
	}

	if (!InAttackRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	OnReadyToAttack();
}

void CAttackEventData::OnMotionDone(DWORD motion, BOOL success)
{
	if (_move_to || _turn_to || !_active_attack_anim)
		return;

	if (motion == _active_attack_anim)
	{
		_active_attack_anim = 0;

		if (success)
		{
			OnAttackAnimSuccess(motion);
		}
		else
		{
			Cancel();
		}
	}
}

void CAttackEventData::OnAttackAnimSuccess(DWORD motion)
{
	Done();
}

void CAttackEventData::Done(DWORD error)
{
	_manager->OnAttackDone(error);
}

bool CAttackEventData::IsValidTarget()
{
	CWeenieObject *target = GetTarget();

	if (!target || !target->IsAttackable() || target->IsDead() || target->IsInPortalSpace() || target->ImmuneToDamage(_weenie))
	{
		return false;
	}

	return true;
}

void CAttackEventData::ExecuteAnimation(DWORD motion, MovementParameters *params)
{
	assert(!_move_to);
	assert(!_turn_to);
	assert(!_active_attack_anim);

	if (_weenie->IsDead() || _weenie->IsInPortalSpace())
	{
		Cancel(WERROR_ACTIONS_LOCKED);
		return;
	}

	_active_attack_anim = motion;

	DWORD error = _weenie->DoForcedMotion(motion, params);

	if (error)
	{
		Cancel(error);
	}
}

void CMeleeAttackEvent::Setup()
{
	DWORD attack_motion = 0;
	DWORD weapon_id = 0;

	if (!_do_attack_animation)
	{
		if (_weenie->_combatTable)
		{
			CWeenieObject *weapon = _weenie->GetWieldedCombat(COMBAT_USE_MELEE);
			if (weapon)
			{
				weapon_id = weapon->GetID();

				AttackType attack_type = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

				if (attack_type == (Thrust_AttackType | Slash_AttackType))
				{
					if (_attack_power >= 0.75f)
						attack_type = Slash_AttackType;
					else
						attack_type = Thrust_AttackType;
				}

				if (CombatManeuver *combat_maneuver = _weenie->_combatTable->TryGetCombatManuever(_weenie->get_minterp()->InqStyle(), attack_type, _attack_height))
				{
					attack_motion = combat_maneuver->motion;
				}
			}
		}

		if (!attack_motion)
		{
			switch (_attack_height)
			{
			case LOW_ATTACK_HEIGHT: attack_motion = Motion_AttackLow1; break;
			case MEDIUM_ATTACK_HEIGHT: attack_motion = Motion_AttackMed1; break;
			case HIGH_ATTACK_HEIGHT: attack_motion = Motion_AttackHigh1; break;
			default:
				{
					Cancel();
					return;
				}
			}

			if (_attack_power >= 0.25f)
				attack_motion += 3;
			if (_attack_power >= 0.75f)
				attack_motion += 3;

			if (_attack_power < 0.0f || _attack_power > 1.0f)
			{
				Cancel();
				return;
			}
		}
	
		_do_attack_animation = attack_motion;
	}

	int attackTime = max(0, min(120, _weenie->GetAttackTimeUsingWielded()));

	_attack_speed = 1.0 / (1.0 / (1.0 + ((120 - attackTime) * (0.005))));

	CAttackEventData::Setup();
}

void CMeleeAttackEvent::OnReadyToAttack()
{
	if (_do_attack_animation)
	{
		MovementParameters params;
		params.sticky = 1;
		params.can_charge = 1;
		params.modify_interpreted_state = 1;
		params.speed = _attack_speed;
		params.action_stamp = ++_weenie->m_wAnimSequence;
		params.autonomous = 0;
		_weenie->stick_to_object(_target_id);

		ExecuteAnimation(_do_attack_animation, &params);
	}
	else
	{
		Finish();
	}
}

void CMeleeAttackEvent::OnAttackAnimSuccess(DWORD motion)
{
	Finish();
}

void CMeleeAttackEvent::Finish()
{
	CWeenieObject *target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	Done();
}

void CMeleeAttackEvent::HandleAttackHook(const AttackCone &cone)
{
	CWeenieObject *target = GetTarget();

	if (!target || !IsValidTarget())
	{
		return;
	}

	DamageEventData dmgEvent;
	dmgEvent.source = _weenie;
	dmgEvent.target = target;
	dmgEvent.damage_form = DF_MELEE;

	DWORD baseDamage;
	float variance;

	STypeSkill weaponSkill = STypeSkill::UNDEF_SKILL;

	double offenseMod = 1.0;

	double critChance = 0.1;
	double critMultiplier = 2.0;
	
	DWORD imbuedEffects = 0;

	CWeenieObject *weapon = _weenie->GetWieldedCombat(COMBAT_USE_MELEE);
	if (weapon && weapon->GetAttackDamage())
	{
		imbuedEffects = weapon->GetImbuedEffects();

		dmgEvent.weapon = weapon;
		baseDamage = weapon->GetAttackDamage();
		offenseMod = weapon->GetOffenseMod();
		variance = weapon->InqFloatQuality(DAMAGE_VARIANCE_FLOAT, 0.0f);
		weaponSkill = SkillTable::OldToNewSkill((STypeSkill)weapon->InqIntQuality(WEAPON_SKILL_INT, UNDEF_SKILL, TRUE));

		dmgEvent.damage_type = weapon->InqDamageType();

		if (dmgEvent.damage_type == (DAMAGE_TYPE::SLASH_DAMAGE_TYPE|DAMAGE_TYPE::PIERCE_DAMAGE_TYPE))
		{
			if (_attack_power >= 0.75f)
				dmgEvent.damage_type = DAMAGE_TYPE::SLASH_DAMAGE_TYPE;
			else
				dmgEvent.damage_type = DAMAGE_TYPE::PIERCE_DAMAGE_TYPE;
		}

		dmgEvent.ignoreMagicResist = _weenie->InqBoolQuality(IGNORE_MAGIC_RESIST_BOOL, FALSE) || weapon->InqBoolQuality(IGNORE_MAGIC_RESIST_BOOL, FALSE);
		dmgEvent.ignoreMagicArmor = _weenie->InqBoolQuality(IGNORE_MAGIC_ARMOR_BOOL, FALSE) || weapon->InqBoolQuality(IGNORE_MAGIC_ARMOR_BOOL, FALSE);

		critChance = weapon->GetCriticalFrequency();
		critMultiplier = weapon->GetCriticalMultiplier();
	}
	else
	{
		/*
		DWORD strength = 10;
		m_Qualities.InqAttribute(STRENGTH_ATTRIBUTE, strength, FALSE);
		float bonusFistDamage = (strength > 100) ? (strength - 100) / 20.0f : 0.0f;
		baseDamage = 4 + bonusFistDamage;
		variance = 0.5f;

		dmgEvent.damage_type = BLUDGEON_DAMAGE_TYPE;
		*/

		baseDamage = 0;
		variance = 0;
		dmgEvent.damage_type = BLUDGEON_DAMAGE_TYPE;
		weaponSkill = LIGHT_WEAPONS_SKILL;

		if (_weenie->m_Qualities._body)
		{
			BodyPart *part = _weenie->m_Qualities._body->_body_part_table.lookup(cone.part_index);
			if (part)
			{
				dmgEvent.damage_type = part->_dtype;
				baseDamage = part->_dval * (1 + part->_dvar);
				variance = part->_dvar;
			}
		}

		baseDamage += _weenie->GetAttackDamage();

		offenseMod = _weenie->GetOffenseMod();

		dmgEvent.ignoreMagicResist = _weenie->InqBoolQuality(IGNORE_MAGIC_RESIST_BOOL, FALSE) ? true : false;
		dmgEvent.ignoreMagicArmor = _weenie->InqBoolQuality(IGNORE_MAGIC_ARMOR_BOOL, FALSE) ? true : false;
	}

	DWORD skillLevel = 0;
	if (_weenie->InqSkill(weaponSkill, skillLevel, FALSE))
	{
		skillLevel = (DWORD)(skillLevel * offenseMod);

		/*
		if (skillLevel >= 55)
		{
			baseDamage += (DWORD)((skillLevel - 55) * 0.11);
		}		
		*/
		if (skillLevel >= 100)
		{
			baseDamage += (DWORD)((skillLevel - 100) * 0.11);
		}
	}

	dmgEvent.weaponSkill = skillLevel;

	DWORD meleeDefense = 0;
	if (target->InqSkill(MELEE_DEFENSE_SKILL, meleeDefense, FALSE) && meleeDefense > 0)
	{
		if (target->TryMeleeEvade(skillLevel))
		{
			// send evasion message
			BinaryWriter attackerEvadeEvent;
			attackerEvadeEvent.Write<DWORD>(0x01B3);
			attackerEvadeEvent.WriteString(target->GetName());
			_weenie->SendNetMessage(&attackerEvadeEvent, PRIVATE_MSG, TRUE, FALSE);

			BinaryWriter attackedEvadeEvent;
			attackedEvadeEvent.Write<DWORD>(0x01B4);
			attackedEvadeEvent.WriteString(_weenie->GetName());
			target->SendNetMessage(&attackedEvadeEvent, PRIVATE_MSG, TRUE, FALSE);
			return;
		}
	}

	// if (weaponSkill == UNARMED_COMBAT_SKILL || weaponSkill == FINESSE_WEAPONS_SKILL)
	{
		DWORD strength = 0;
		if (_weenie->m_Qualities.InqAttribute(STRENGTH_ATTRIBUTE, strength, FALSE))
		{
			baseDamage += strength / 20;
		}
	}

	if (imbuedEffects & CripplingBlow_ImbuedEffectType)
	{
		critMultiplier *= _weenie->GetMeleeCripplingBlowMultiplier(skillLevel);
	}

	if (critMultiplier > 7.0)
	{
		critMultiplier = 7.0;
	}

	if (imbuedEffects & CriticalStrike_ImbuedEffectType)
	{
		critChance = _weenie->GetMeleeCriticalStrikeFrequency(skillLevel);
	}

	if (imbuedEffects & IgnoreAllArmor_ImbuedEffectType)
	{
		dmgEvent.ignoreArmorEntirely = true;
	}

	//if (!_weenie->AsPlayer())
	//	baseDamage *= 2;

	bool bCrit = (Random::GenFloat(0.0, 1.0) < critChance) ? true : false;

	double damageCalc;

	damageCalc = baseDamage;
	damageCalc *= 1.0f - Random::GenFloat(0.0f, variance);
	damageCalc *= 0.5 + _attack_power;

	if (bCrit)
		damageCalc *= critMultiplier;

	// damageCalc += 1.0; // free damage
	damageCalc += 0.5f; // round it properly

	dmgEvent.wasCrit = bCrit;
	dmgEvent.outputDamage = dmgEvent.inputDamage = (DWORD)damageCalc;

	dmgEvent.hit_quadrant = (DAMAGE_QUADRANT) _attack_height;

	_weenie->TryToDealDamage(dmgEvent);
}


void CMissileAttackEvent::Setup()
{
	if (_attack_charge_time >= 0.0)
	{
		_attack_charge_time += 1; // for reload animation
	}

	DWORD attack_motion = 0;
	DWORD weapon_id = 0;

	if (!_do_attack_animation)
	{
		/*
		if (_weenie->_combatTable)
		{
			CWeenieObject *weapon = _weenie->GetWieldedCombat(COMBAT_USE_MISSILE);
			if (weapon)
			{
				weapon_id = weapon->GetID();

				AttackType attack_type = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

				if (CombatManeuver *combat_maneuver = _weenie->_combatTable->TryGetCombatManuever(_weenie->get_minterp()->InqStyle(), attack_type, _attack_height))
				{
					attack_motion = combat_maneuver->motion;
				}
			}
		}
		*/

		if (!attack_motion)
		{
			switch (_attack_height)
			{
			case LOW_ATTACK_HEIGHT: attack_motion = Motion_AimLevel; break;
			case MEDIUM_ATTACK_HEIGHT: attack_motion = Motion_AimLevel; break;
			case HIGH_ATTACK_HEIGHT: attack_motion = Motion_AimLevel; break;
			default:
				{
					Cancel();
					return;
				}
			}

			if (_attack_power < 0.0f || _attack_power > 1.0f)
			{
				Cancel();
				return;
			}
		}

		_do_attack_animation = attack_motion;
	}

	int attackTime = max(0, min(120, _weenie->GetAttackTimeUsingWielded()));

	_attack_speed = 1.0 / (1.0 / (1.0 + ((120 - attackTime) * (0.005))));

	CAttackEventData::Setup();

	_max_attack_distance = 60.0;
	_max_attack_angle = MAX_MISSILE_ATTACK_CONE_ANGLE;
	_timeout = Timer::cur_time + 15.0;
	_use_sticky = false;
}

void CMissileAttackEvent::OnReadyToAttack()
{
	if (_do_attack_animation)
	{
		MovementParameters params;
		params.modify_interpreted_state = 1;
		params.speed = _attack_speed;
		params.autonomous = 0;

		ExecuteAnimation(_do_attack_animation, &params);
	}
	else
	{
		Finish();
	}
}

bool CMissileAttackEvent::CalculateTargetPosition()
{
	CWeenieObject *target = GetTarget();
	assert(target);

	if (!target || !target->InValidCell())
	{
		return false;
	}

	_missile_target_position = target->GetPosition();

	switch (_attack_height)
	{
	case ATTACK_HEIGHT::LOW_ATTACK_HEIGHT:
		_missile_target_position.frame.m_origin.z += target->GetHeight() * (1.0 / 6.0); // 0.25;
		break;

	default:
	case ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT:
		_missile_target_position.frame.m_origin.z += target->GetHeight() * 0.5;
		break;

	case ATTACK_HEIGHT::HIGH_ATTACK_HEIGHT:
		_missile_target_position.frame.m_origin.z += target->GetHeight() * (5.0 / 6.0); // 0.75;
		break;
	}

	return true;
}

bool CMissileAttackEvent::CalculateSpawnPosition(float missileRadius)
{
	if (!_weenie->InValidCell())
	{
		return false;
	}

	_missile_spawn_position = _weenie->GetPosition();
	_missile_spawn_position = _missile_spawn_position.add_offset(Vector(0, 0, _weenie->GetHeight() * 0.75)); //(2.0 / 3.0))); // 0.75f));
	
	Vector targetOffset = _missile_spawn_position.get_offset(_missile_target_position);
	Vector targetDir = targetOffset;

	if (targetDir.normalize_check_small())
	{
		targetDir = _missile_spawn_position.frame.get_vector_heading();

		// spawnPosition.frame.m_origin += targetDir * minSpawnDist;
		_missile_spawn_position.frame.set_vector_heading(targetDir);
		_missile_dist_to_target = 0.0;
	}
	else
	{
		float minSpawnDist = (_weenie->GetRadius() + missileRadius) + 0.1f;

		_missile_spawn_position.frame.m_origin += targetDir * minSpawnDist;
		_missile_spawn_position.frame.set_vector_heading(targetDir);

		_missile_dist_to_target = targetOffset.magnitude();
	}

	return true;
}

bool CMissileAttackEvent::CalculateMissileVelocity(bool track, bool gravity, float speed)
{
	CWeenieObject *target = GetTarget();

	if (!target)
	{
		return false;
	}

	Vector targetOffset = _missile_spawn_position.get_offset(_missile_target_position);
	double targetDist = targetOffset.magnitude();

	if (!track)
	{
		double t = targetDist / speed;
		Vector v = targetOffset / t;

		if (gravity)
		{
			v.z += (9.8*t) / 2.0f;
		}

		//Vector targetDir = v;
		//targetDir.normalize();

		_missile_velocity = v;
		return true;
	}

	Vector P0 = targetOffset;
	Vector P1(0, 0, 0);

	float s0 = target->get_velocity().magnitude();
	Vector V0 = target->get_velocity();
	if (V0.normalize_check_small())
	{
		V0 = Vector(0, 0, 0);
	}

	float s1 = speed;

	double a = (V0.x * V0.x) + (V0.y * V0.y) - (s1 * s1);
	double b = 2 * ((P0.x * V0.x) + (P0.y * V0.y) - (P1.x * V0.x) - (P1.y * V0.y));
	double c = (P0.x * P0.x) + (P0.y * P0.y) + (P1.x * P1.x) + (P1.y * P1.y) - (2 * P1.x * P0.x) - (2 * P1.y * P0.y);

	double t1 = (-b + sqrt((b * b) - (4 * a * c))) / (2 * a);
	double t2 = (-b - sqrt((b * b) - (4 * a * c))) / (2 * a);

	if (t1 < 0)
	{
		t1 = FLT_MAX;
	}

	if (t2 < 0)
	{
		t2 = FLT_MAX;
	}

	double t = min(t1, t2);
	if (t >= 100.0)
	{
		return CalculateMissileVelocity(false, true, speed);
	}

	Vector v;
	v.x = (P0.x + (t * s0 * V0.x)) / (t); // * s1);
	v.y = (P0.y + (t * s0 * V0.y)) / (t); // * s1);
	v.z = (P0.z + (t * s0 * V0.z)) / (t); // * s1);

	if (gravity)
	{
		// add z to velocity for gravity
		v.z += (9.8*t) / 2.0f;
	}

	_missile_velocity = v;

	return true;
}

void CMissileAttackEvent::FireMissile()
{
	CWeenieObject *launcher = _weenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_MISSILE);

	if (!launcher)
	{
		_weenie->DoForcedStopCompletely();
		return;
	}

	CWeenieObject *ammo = _weenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_AMMO);

	if (!ammo)
	{
		_weenie->DoForcedStopCompletely();
		return;
	}

	CWeenieObject *missile = g_pWeenieFactory->CloneWeenie(ammo);

	if (!missile)
	{
		_weenie->DoForcedStopCompletely();
		return;
	}

	int stackSize = 0;
	if (missile->m_Qualities.InqInt(STACK_SIZE_INT, stackSize))
	{
		missile->SetStackSize(1);
	}

	missile->m_Qualities.SetInstanceID(WIELDER_IID, 0);
	missile->m_Qualities.SetInstanceID(CONTAINER_IID, 0);
	missile->_cachedHasOwner = false;

	missile->m_Qualities.SetInt(CURRENT_WIELDED_LOCATION_INT, 0);
	missile->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);

	missile->SetInitialPhysicsState(INELASTIC_PS | GRAVITY_PS | PATHCLIPPED_PS | ALIGNPATH_PS | MISSILE_PS | REPORT_COLLISIONS_PS);
	missile->SetInitialPosition(_weenie->GetPosition());
	missile->InitPhysicsObj();

	CalculateTargetPosition();
	CalculateSpawnPosition(missile->GetRadius());
	CalculateMissileVelocity(true, true, launcher->InqFloatQuality(MAXIMUM_VELOCITY_FLOAT, 20.0));

	missile->m_Position = _missile_spawn_position;
	missile->set_velocity(_missile_velocity, FALSE);

	if (missile->AsAmmunition())
	{
		CWeenieObject *launcher = _weenie->GetWieldedCombat(COMBAT_USE_MISSILE);
		CWeenieObject *target = GetTarget();
		missile->AsAmmunition()->_sourceID = _weenie->GetID();
		missile->AsAmmunition()->_launcherID = launcher ? launcher->GetID() : 0;
		missile->AsAmmunition()->_targetID = target ? target->GetID() : 0;
		missile->AsAmmunition()->_attackPower = _attack_power;
	}

	if (g_pWorld->CreateEntity(missile))
	{
		missile->EmitEffect(PS_Launch, 0.0f);
	}

	int weaponSkill = launcher->InqIntQuality(AMMO_TYPE_INT, 0);

	switch (weaponSkill)
	{
	case AMMO_TYPE::AMMO_ARROW:
	case AMMO_TYPE::AMMO_ARROW_CHORIZITE:
	case AMMO_TYPE::AMMO_ARROW_CRYSTAL:
		_weenie->EmitSound(Sound_BowRelease, 1.0f);
		break;

	case AMMO_TYPE::AMMO_BOLT:
	case AMMO_TYPE::AMMO_BOLT_CHORIZITE:
	case AMMO_TYPE::AMMO_BOLT_CRYSTAL:
		_weenie->EmitSound(Sound_CrossbowRelease, 1.0f);
		break;
	}

	if (ammo->IsWorldAware())
	{
		ammo->_position_timestamp++;

		BinaryWriter removeFrom3D;
		removeFrom3D.Write<DWORD>(0xF74A);
		removeFrom3D.Write<DWORD>(ammo->GetID());
		removeFrom3D.Write<WORD>(ammo->_instance_timestamp);
		removeFrom3D.Write<WORD>(ammo->_position_timestamp);
		g_pWorld->BroadcastPVS(ammo->GetWorldTopLevelOwner()->GetLandcell(), removeFrom3D.GetData(), removeFrom3D.GetSize());
	}

	ammo->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
	ammo->unset_parent();

	_weenie->DoForcedMotion(Motion_Reload);
}

void CMissileAttackEvent::OnAttackAnimSuccess(DWORD motion)
{
	FireMissile();
	Finish();
}

void CMissileAttackEvent::Finish()
{
	CWeenieObject *target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	Done();
}

void CMissileAttackEvent::HandleAttackHook(const AttackCone &cone)
{
}

AttackManager::AttackManager(CWeenieObject *weenie)
{
	_weenie = weenie;
}

AttackManager::~AttackManager()
{
	SafeDelete(_attackData);
	SafeDelete(_cleanupData);
}

void AttackManager::MarkForCleanup(CAttackEventData *data)
{
	if (_cleanupData && _cleanupData != data)
	{
		delete _cleanupData;
	}

	_cleanupData = data;
}

void AttackManager::Cancel()
{
	if (_attackData)
	{
		_attackData->Cancel();
	}
}

void AttackManager::OnAttackCancelled(DWORD error)
{
	if (_attackData)
	{
		_weenie->NotifyAttackDone();
		_weenie->unstick_from_object();

		MarkForCleanup(_attackData);
		_attackData = NULL;
	}
}

bool AttackManager::RepeatAttacks()
{
	if (_weenie->AsPlayer())
	{
		CPlayerWeenie *player = (CPlayerWeenie *)_weenie;
		return player->ShouldRepeatAttacks();
	}

	return false;
}

void AttackManager::OnAttackDone(DWORD error)
{
	bool bRepeatAttack = false;

	if (_attackData)
	{
		if (RepeatAttacks() && _attackData->IsValidTarget())
		{
			_weenie->NotifyAttackDone();
			
			_weenie->NotifyCommenceAttack();

			_attackData->_attack_charge_time = Timer::cur_time + (_attackData->_attack_power * 2);
			_attackData->Begin();
		}
		else
		{
			_weenie->NotifyAttackDone();

			MarkForCleanup(_attackData);
			_attackData = NULL;
		}
	}
}

void AttackManager::Update()
{
	if (_attackData)
	{
		_attackData->Update();
	}

	SafeDelete(_cleanupData);
}

void AttackManager::OnDeath(DWORD killer_id)
{
	Cancel();
}

void AttackManager::HandleMoveToDone(DWORD error)
{
	if (_attackData)
	{
		_attackData->HandleMoveToDone(error);
	}
}

void AttackManager::HandleAttackHook(const AttackCone &cone)
{
	if (_attackData)
	{
		_attackData->HandleAttackHook(cone);
	}
}

void AttackManager::OnMotionDone(DWORD motion, BOOL success)
{
	if (_attackData)
	{
		_attackData->OnMotionDone(motion, success);
	}
}

bool AttackManager::IsAttacking()
{
	return _attackData != NULL ? true : false;
}

void AttackManager::BeginAttack(CAttackEventData *data)
{
	if (_attackData && _attackData != data)
	{
		// already busy
		_weenie->NotifyWeenieError(WERROR_ACTIONS_LOCKED);

		delete data;
		return;
	}

	_attackData = data;
	_attackData->Begin();
}

void AttackManager::BeginMeleeAttack(DWORD target_id, ATTACK_HEIGHT height, float power, float chase_distance, DWORD motion)
{
	CMeleeAttackEvent *attackEvent = new CMeleeAttackEvent();

	attackEvent->_weenie = _weenie;
	attackEvent->_manager = this;
	attackEvent->_target_id = target_id;
	attackEvent->_attack_height = height;
	attackEvent->_attack_power = power;
	attackEvent->_do_attack_animation = motion;
	attackEvent->_fail_distance = chase_distance;

	BeginAttack(attackEvent);
}

void AttackManager::BeginMissileAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion)
{
	CMissileAttackEvent *attackEvent = new CMissileAttackEvent();

	attackEvent->_weenie = _weenie;
	attackEvent->_manager = this;
	attackEvent->_target_id = target_id;
	attackEvent->_attack_height = height;
	attackEvent->_attack_power = power;
	attackEvent->_do_attack_animation = motion;

	BeginAttack(attackEvent);
}





