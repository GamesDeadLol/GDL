
#include "StdAfx.h"
#include "MonsterAI.h"
#include "WeenieObject.h"
#include "Monster.h"
#include "World.h"
#include "SpellcastingManager.h"
#include "EmoteManager.h"

#define DEFAULT_AWARENESS_RANGE 40.0

MonsterAIManager::MonsterAIManager(CMonsterWeenie *pWeenie, const Position &HomePos)
{
	m_pWeenie = pWeenie;
	_toleranceType = pWeenie->InqIntQuality(TOLERANCE_INT, 0, TRUE);
	_cachedVisualAwarenessRange = m_pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE);

	// SetHomePosition(HomePos); don't use preset home position
}

MonsterAIManager::~MonsterAIManager()
{
}

void MonsterAIManager::SetHomePosition(const Position &pos)
{
	m_HomePosition = pos;
}

void MonsterAIManager::Update()
{
	if (!m_HomePosition.objcell_id)
	{
		// make sure we set a home position
		if (!(m_pWeenie->transient_state & ON_WALKABLE_TS))
			return;

		SetHomePosition(m_pWeenie->m_Position);
	}

	switch (m_State)
	{
	case MonsterAIState::Idle:
		UpdateIdle();
		break;

	case MonsterAIState::MeleeModeAttack:
		UpdateMeleeModeAttack();
		break;

	case MonsterAIState::ReturningToSpawn:
		UpdateReturningToSpawn();
		break;

	case MonsterAIState::SeekNewTarget:
		UpdateSeekNewTarget();
		break;
	}
}

void MonsterAIManager::SwitchState(int state)
{
	if (state == m_State)
		return;

	switch (state)
	{
	case MonsterAIState::Idle:
		EndIdle();
		break;

	case MonsterAIState::MeleeModeAttack:
		EndMeleeModeAttack();
		break;

	case MonsterAIState::ReturningToSpawn:
		EndReturningToSpawn();
		break;

	case MonsterAIState::SeekNewTarget:
		EndSeekNewTarget();
		break;
	}

	EnterState(state);
}

void MonsterAIManager::EnterState(int state)
{
	m_State = state;

	switch (state)
	{
	case MonsterAIState::Idle:
		BeginIdle();
		break;

	case MonsterAIState::MeleeModeAttack:
		BeginMeleeModeAttack();
		break;

	case MonsterAIState::ReturningToSpawn:
		BeginReturningToSpawn();
		break;

	case MonsterAIState::SeekNewTarget:
		BeginSeekNewTarget();
		break;
	}
}

void MonsterAIManager::BeginIdle()
{
	m_fNextPVSCheck = Timer::cur_time;
	m_pWeenie->ChangeCombatMode(COMBAT_MODE::NONCOMBAT_COMBAT_MODE, false);
}

void MonsterAIManager::EndIdle()
{
}

void MonsterAIManager::UpdateIdle()
{
	if (_toleranceType == TolerateNothing)
	{
		SeekTarget();
	}
}

bool MonsterAIManager::SeekTarget()
{
	if (m_fNextPVSCheck <= Timer::cur_time)
	{
		m_fNextPVSCheck = Timer::cur_time + 2.0f;

		std::list<CWeenieObject *> results;
		g_pWorld->EnumNearbyPlayers(m_pWeenie, _cachedVisualAwarenessRange, &results); // m_HomePosition

		std::list<CWeenieObject *> validTargets;

		CWeenieObject *pClosestWeenie = NULL;
		double fClosestWeenieDist = FLT_MAX;

		for (auto weenie : results)
		{
			if (weenie == m_pWeenie)
				continue;

			if (!weenie->_IsPlayer()) // only attack players
				continue;

			if (!weenie->IsAttackable())
				continue;

			if (weenie->ImmuneToDamage(m_pWeenie)) // only attackable players (not dead, not in portal space, etc.
				continue;

			validTargets.push_back(weenie);

			/*
			double fWeenieDist = m_pWeenie->DistanceTo(weenie);
			if (pClosestWeenie && fWeenieDist >= fClosestWeenieDist)
			continue;

			pClosestWeenie = weenie;
			fClosestWeenieDist = fWeenieDist;
			*/
		}

		/*
		if (pClosestWeenie)
		SetNewTarget(pClosestWeenie);
		*/

		if (!validTargets.empty())
		{
			// Random target
			std::list<CWeenieObject *>::iterator i = validTargets.begin();
			std::advance(i, Random::GenInt(0, (unsigned int)(validTargets.size() - 1)));
			SetNewTarget(*i);
			return true;
		}
	}

	return false;
}

void MonsterAIManager::SetNewTarget(CWeenieObject *pTarget)
{
	m_TargetID = pTarget->GetID();

	SwitchState(MeleeModeAttack);

	m_pWeenie->ChanceExecuteEmoteSet(pTarget->GetID(), NewEnemy_EmoteCategory);
}

void MonsterAIManager::BeginMeleeModeAttack()
{
	m_pWeenie->ChangeCombatMode(COMBAT_MODE::MELEE_COMBAT_MODE, false);

	m_fChaseTimeoutTime = Timer::cur_time + m_fChaseTimeoutDuration;
	m_fNextAttackTime = Timer::cur_time;
	m_fNextChaseTime = Timer::cur_time;
	m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;
	m_fMinReturnStateTime = Timer::cur_time + m_fMinReturnStateDuration;
}

CWeenieObject *MonsterAIManager::GetTargetWeenie()
{
	return g_pWorld->FindObject(m_TargetID);
}

float MonsterAIManager::DistanceToHome()
{
	if (!m_HomePosition.objcell_id)
		return FLT_MAX;

	return m_HomePosition.distance(m_pWeenie->m_Position);
}

bool MonsterAIManager::ShouldSeekNewTarget()
{
	if (DistanceToHome() >= m_fMaxHomeRange)
		return false;

	return true;
}

void MonsterAIManager::UpdateMeleeModeAttack()
{
	if (m_pWeenie->IsBusyOrInAction())
	{
		// still animating or busy (attacking, etc.)
		return;
	}

	// rules:
	// dont switch targets to one that is farther than visual awareness range, unless attacked
	// dont chase a target that is outside the chase range, unless attacked
	// dont chase any new target, even if attacked, outside home range

	CWeenieObject *pTarget = GetTargetWeenie();	
	if (!pTarget || pTarget->IsDead() || !pTarget->IsAttackable() || pTarget->ImmuneToDamage(m_pWeenie) || m_pWeenie->DistanceTo(pTarget) >= m_fChaseRange)
	{
		if (ShouldSeekNewTarget())
		{
			SwitchState(SeekNewTarget);
		}
		else
		{
			SwitchState(ReturningToSpawn);
		}
		return;
	}

	if (DistanceToHome() >= m_fMaxHomeRange)
	{
		SwitchState(ReturningToSpawn);
		return;
	}

	double fTargetDist = m_pWeenie->DistanceTo(pTarget, true);
	if (fTargetDist >= m_pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE) && m_fAggroTime <= Timer::cur_time)
	{
		SwitchState(ReturningToSpawn);
		return;
	}

	if (m_fNextAttackTime > Timer::cur_time)
	{
		return;
	}

	if (m_pWeenie->DistanceTo(pTarget) > 30.0f || !RollDiceCastSpell())
	{
		// do physics attack
		DWORD motion = 0;
		ATTACK_HEIGHT height = ATTACK_HEIGHT::UNDEF_ATTACK_HEIGHT;
		float power = 0.0f;
		GenerateRandomAttack(&motion, &height, &power);
		m_pWeenie->TryMeleeAttack(pTarget->GetID(), height, power, motion);

		m_fNextAttackTime = Timer::cur_time + 2.0f;
		m_fNextChaseTime = Timer::cur_time; // chase again anytime
		m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;
	}
}

bool MonsterAIManager::RollDiceCastSpell()
{
	if (m_fNextCastTime > Timer::cur_time)
	{
		return false;
	}

	if (m_pWeenie->m_Qualities._spell_book)
	{
		/* not correct, these must be independent events (look at wisps)
		float dice = Random::RollDice(0.0f, 1.0f);

		auto spellIterator = m_pWeenie->m_Qualities._spell_book->_spellbook.begin();

		while (spellIterator != m_pWeenie->m_Qualities._spell_book->_spellbook.end())
		{
			float likelihood = spellIterator->second._casting_likelihood;

			if (dice <= likelihood)
			{
				return DoCastSpell(spellIterator->first);
			}

			dice -= likelihood;
			spellIterator++;
		}
		*/
		
		auto spellIterator = m_pWeenie->m_Qualities._spell_book->_spellbook.begin();

		while (spellIterator != m_pWeenie->m_Qualities._spell_book->_spellbook.end())
		{
			float dice = Random::RollDice(0.0f, 1.0f);
			float likelihood = spellIterator->second._casting_likelihood;

			if (dice <= likelihood)
			{
				m_fNextCastTime = Timer::cur_time + m_pWeenie->m_Qualities.GetFloat(AI_USE_MAGIC_DELAY_FLOAT, 0.0);
				return DoCastSpell(spellIterator->first);
			}

			spellIterator++;
		}
	}

	return false;
}

bool MonsterAIManager::DoCastSpell(DWORD spell_id)
{	
	CWeenieObject *pTarget = GetTargetWeenie();
	m_pWeenie->MakeSpellcastingManager()->CreatureBeginCast(pTarget ? pTarget->GetID() : 0, spell_id);
	return true;
}

bool MonsterAIManager::DoMeleeAttack()
{
	DWORD motion = 0;
	ATTACK_HEIGHT height = ATTACK_HEIGHT::UNDEF_ATTACK_HEIGHT;
	float power = 0.0f;
	GenerateRandomAttack(&motion, &height, &power);
	if (!motion)
	{
		return false;
	}

	CWeenieObject *pTarget = GetTargetWeenie();
	if (!pTarget)
	{
		return false;
	}

	m_pWeenie->TryMeleeAttack(pTarget->GetID(), height, power, motion);

	m_fNextAttackTime = Timer::cur_time + 2.0f;
	m_fNextChaseTime = Timer::cur_time; // chase again anytime
	m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;

	return true;
}

void MonsterAIManager::GenerateRandomAttack(DWORD *motion, ATTACK_HEIGHT *height, float *power)
{
	*motion = 0;
	*height = ATTACK_HEIGHT::UNDEF_ATTACK_HEIGHT;
	*power = Random::GenFloat(0, 1);

	if (m_pWeenie->_combatTable)
	{
		CWeenieObject *weapon = m_pWeenie->GetWieldedCombat(COMBAT_USE_MELEE);
		if (weapon)
		{
			AttackType attackType = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

			if (attackType == (Thrust_AttackType | Slash_AttackType))
			{
				if (*power >= 0.75f)
					attackType = Slash_AttackType;
				else
					attackType = Thrust_AttackType;
			}

			CombatManeuver *combatManeuver;
			
			// some monster have undef'd attack heights (hollow?) which is index 0
			combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(0, 3));

			if (!combatManeuver)
			{
				// and some don't
				combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(1, 3));

				if (!combatManeuver)
				{
					combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::HIGH_ATTACK_HEIGHT);
				
					if (!combatManeuver)
					{
						combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT);
				
						if (!combatManeuver)
						{
							combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::LOW_ATTACK_HEIGHT);
						}
					}
				}
			}

			if (combatManeuver)
			{
				*motion = combatManeuver->motion;
			}
		}
		else
		{
			AttackType attackType;
			
			if (*power >= 0.75f)
			{
				attackType = Kick_AttackType;
			}
			else
			{
				attackType = Punch_AttackType;
			}

			CombatManeuver *combatManeuver;

			// some monster have undef'd attack heights (hollow?) which is index 0
			combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(0, 3));

			if (!combatManeuver)
			{
				// and some don't
				combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(1, 3));

				if (!combatManeuver)
				{
					combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::HIGH_ATTACK_HEIGHT);

					if (!combatManeuver)
					{
						combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT);

						if (!combatManeuver)
						{
							combatManeuver = m_pWeenie->_combatTable->TryGetCombatManuever(m_pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::LOW_ATTACK_HEIGHT);
						}
					}
				}
			}

			if (combatManeuver)
			{
				*motion = combatManeuver->motion;
			}
		}
	}

	if (!*motion)
	{
		*motion = Motion_AttackHigh1;
	}
}

void MonsterAIManager::EndMeleeModeAttack()
{
	m_pWeenie->unstick_from_object();
}

void MonsterAIManager::BeginReturningToSpawn()
{
	// m_pWeenie->DoForcedStopCompletely();

	MovementParameters params;
	params.can_walk = 0;

	MovementStruct mvs;
	mvs.type = MovementTypes::MoveToPosition;	
	mvs.pos = m_HomePosition;
	mvs.params = &params;

	m_pWeenie->movement_manager->PerformMovement(mvs);

	m_fReturnTimeoutTime = Timer::cur_time + m_fReturnTimeout;
}

void MonsterAIManager::EndReturningToSpawn()
{
}

void MonsterAIManager::UpdateReturningToSpawn()
{
	float fDistToHome = m_HomePosition.distance(m_pWeenie->m_Position);

	if (fDistToHome < 5.0f)
	{
		SwitchState(Idle);
		return;
	}
	
	if (m_fReturnTimeoutTime <= Timer::cur_time)
	{
		// teleport back to spawn
		m_pWeenie->Movement_Teleport(m_HomePosition);

		SwitchState(Idle);
		return;
	}
}

void MonsterAIManager::OnDeath()
{
}

bool MonsterAIManager::IsValidTarget(CWeenieObject *pWeenie)
{
	if (!pWeenie)
		return false;

	if (pWeenie == m_pWeenie)
		return false;

	if (!pWeenie->_IsPlayer()) // only attack players
		return false;

	if (pWeenie->ImmuneToDamage(m_pWeenie)) // only attackable players (not dead, not in portal space, etc.
		return false;

	return true;
}

void MonsterAIManager::AlertIdleFriendsToAggro(CWeenieObject *pAttacker)
{
	std::list<CWeenieObject *> results;
	g_pWorld->EnumNearby(m_pWeenie, 20.0f, &results);
	
	for (auto weenie : results)
	{
		if (weenie == m_pWeenie)
			continue;

		if (!weenie->IsCreature())
			continue;

		CMonsterWeenie *creature = (CMonsterWeenie *)weenie;
		if (!creature->m_MonsterAI)
			continue;

		switch (creature->m_MonsterAI->m_State)
		{
		case Idle:
		case ReturningToSpawn:
		case SeekNewTarget:
			break;

		default:
			continue;
		}

		if (!creature->m_MonsterAI->IsValidTarget(pAttacker))
			continue;

		if (creature->m_MonsterAI->_toleranceType == TolerateEverything)
			continue;

		creature->m_MonsterAI->m_fAggroTime = Timer::cur_time + 10.0;
		creature->m_MonsterAI->SetNewTarget(pAttacker);
	}
}

void MonsterAIManager::OnTookDamage(CWeenieObject *pAttacker, unsigned int damage)
{
	HandleAggro(pAttacker);

	if (m_pWeenie->m_Qualities._emote_table && !m_pWeenie->IsExecutingEmote())
	{
		PackableList<EmoteSet> *emoteSetList = m_pWeenie->m_Qualities._emote_table->_emote_table.lookup(WoundedTaunt_EmoteCategory);

		if (emoteSetList)
		{
			double healthPercent = m_pWeenie->GetHealthPercent();
			if (m_fLastWoundedTauntHP > healthPercent)
			{
				double dice = Random::GenFloat(0.0, 1.0);

				for (auto &emoteSet : *emoteSetList)
				{
					// ignore probability?
					if (healthPercent >= emoteSet.minhealth && healthPercent < emoteSet.maxhealth)
					{
						m_fLastWoundedTauntHP = healthPercent;

						m_pWeenie->MakeEmoteManager()->ExecuteEmoteSet(emoteSet, pAttacker->GetID());
					}
				}
			}
		}
	}
}

void MonsterAIManager::OnIdentifyAttempted(CWeenieObject *other)
{
	if (_toleranceType != TolerateUnlessBothered)
	{
		return;
	}

	if (m_pWeenie->DistanceTo(other, true) >= 60.0)
	{
		return;
	}

	HandleAggro(other);
}

void MonsterAIManager::HandleAggro(CWeenieObject *pAttacker)
{
	if (_toleranceType == TolerateEverything)
	{
		return;
	}

	if (!m_pWeenie->IsDead())
	{
		switch (m_State)
		{
		case Idle:
		case ReturningToSpawn:
		case SeekNewTarget:
			{
				if (IsValidTarget(pAttacker))
				{
					//if (m_pWeenie->DistanceTo(pAttacker, true) <= m_pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE))
					//{
					SetNewTarget(pAttacker);
					//}

					m_pWeenie->ChanceExecuteEmoteSet(pAttacker->GetID(), Scream_EmoteCategory);
					m_fAggroTime = Timer::cur_time + 10.0;
				}

				break;
			}
		}
	}

	AlertIdleFriendsToAggro(pAttacker);
}

void MonsterAIManager::BeginSeekNewTarget()
{
	if (!ShouldSeekNewTarget() || !SeekTarget())
	{
		SwitchState(ReturningToSpawn);
	}
}

void MonsterAIManager::UpdateSeekNewTarget()
{
	SwitchState(ReturningToSpawn);
}

void MonsterAIManager::EndSeekNewTarget()
{
}


