
#include "StdAfx.h"
#include "WeenieObject.h"
#include "Monster.h"
#include "World.h"
#include "GameMode.h"
#include "Lifestone.h"
#include "ChatMsgs.h"
#include "SpellProjectile.h"
#include "MonsterAI.h"
#include "WeenieFactory.h"
#include "InferredPortalData.h"
#include "ObjectMsgs.h"
#include "EmoteManager.h"
#include "Corpse.h"
#include "AttackManager.h"
#include "WClassID.h"
#include "InferredPortalData.h"
#include "Config.h"
#include "DatabaseIO.h"
#include "InferredPortalData.h"
#include "SpellTableExtendedData.h"
#include "Door.h"
#include "TreasureFactory.h"

CMonsterWeenie::CMonsterWeenie()
{
	SetItemType(TYPE_CREATURE);

	m_Qualities.SetInt(LEVEL_INT, 1);
	m_Qualities.SetAttribute(STRENGTH_ATTRIBUTE, 100);
	m_Qualities.SetAttribute(ENDURANCE_ATTRIBUTE, 100);
	m_Qualities.SetAttribute(COORDINATION_ATTRIBUTE, 100);
	m_Qualities.SetAttribute(QUICKNESS_ATTRIBUTE, 100);
	m_Qualities.SetAttribute(FOCUS_ATTRIBUTE, 100);
	m_Qualities.SetAttribute(SELF_ATTRIBUTE, 100);
	m_Qualities.SetAttribute2nd(MAX_HEALTH_ATTRIBUTE_2ND, 0);
	m_Qualities.SetAttribute2nd(HEALTH_ATTRIBUTE_2ND, 50);
	m_Qualities.SetAttribute2nd(MAX_STAMINA_ATTRIBUTE_2ND, 0);
	m_Qualities.SetAttribute2nd(STAMINA_ATTRIBUTE_2ND, 100);
	m_Qualities.SetAttribute2nd(MAX_MANA_ATTRIBUTE_2ND, 0);
	m_Qualities.SetAttribute2nd(MANA_ATTRIBUTE_2ND, 100);

	m_Qualities.SetInt(SHOWABLE_ON_RADAR_INT, ShowAlways_RadarEnum);

	m_Qualities.SetBool(STUCK_BOOL, TRUE);
	m_Qualities.SetBool(ATTACKABLE_BOOL, TRUE);
	m_Qualities.SetInt(ITEM_USEABLE_INT, USEABLE_NO);
}

CMonsterWeenie::~CMonsterWeenie()
{
	SafeDelete(m_MonsterAI);
}

void CMonsterWeenie::ApplyQualityOverrides()
{
}

void CMonsterWeenie::PreSpawnCreate()
{
	if (m_Qualities._create_list)
	{
		for (auto i = m_Qualities._create_list->begin(); i != m_Qualities._create_list->end(); i++)
		{
			if (i->destination == DestinationType::Wield_DestinationType ||
				i->destination == DestinationType::WieldTreasure_DestinationType)
			{
				// not sure why some wcid's are zero
				if (!i->wcid)
					continue;

				CWeenieObject *wielded = SpawnWielded(i->wcid, i->palette, i->shade);
			}
		}
	}

	DWORD wieldedTreasureDID;
	if (wieldedTreasureDID = m_Qualities.GetDID(WIELDED_TREASURE_TYPE_DID, 0))
	{
		if (PackableList<TreasureEntry> *te = g_pPortalDataEx->_treasureTableData._wieldedTreasure.lookup(wieldedTreasureDID))
		{
			bool bSpawned = false;
			bool bOnlySpawnIfPreviousSpawned = false;

			for (PackableList<TreasureEntry>::iterator i = te->begin(); i != te->end(); i++)
			{
				if (bOnlySpawnIfPreviousSpawned)
				{
					bOnlySpawnIfPreviousSpawned = i->m_b2C ? true : false;
					continue;
				}

				bOnlySpawnIfPreviousSpawned = i->m_b2C ? true : false;

				if (i->m_b28)
				{
					double diceRoll = Random::RollDice(0.0, 1.0);
					double offset = 0.0;

					PackableList<TreasureEntry>::iterator j = i;
					for (; j != te->end(); j++)
					{
						if (j->m_b28 && i != j)
						{
							break;
						}

						if (diceRoll <= (j->_chance + offset))
						{
							if (CWeenieDefaults *def = g_pWeenieFactory->GetWeenieDefaults(j->_wcid))
							{
								if (def->m_Qualities.m_WeenieType != Missile_WeenieType &&  // no support for these yet
									def->m_Qualities.m_WeenieType != MissileLauncher_WeenieType &&
									def->m_Qualities.m_WeenieType != Ammunition_WeenieType)
								{
									// spawn it
									SpawnWielded(j->_wcid, j->_ptid, j->_shade);
									bSpawned = true;
									
									if (j->m_b2C)
									{
										// spawn next too
										for (j++; j != te->end(); j++)
										{
											if (j->m_b28)
											{
												diceRoll = Random::RollDice(0.0, 1.0);
												offset = 0.0;
											}

											if (diceRoll <= (j->_chance + offset))
											{
												SpawnWielded(j->_wcid, j->_ptid, j->_shade);
											}

											if (!j->m_b2C)
												break;

											offset += j->_chance;
										}
									}

									break;
								}
							}
						}

						offset += j->_chance;
					}
				}

				if (bSpawned)
				{
					break;
				}
			}
		}
	}
}

void CMonsterWeenie::PostSpawn()
{
	CWeenieObject::PostSpawn();

	// check if we need to create a monster AI
	if (IsCreature() && !AsPlayer() && !IsVendor() && m_Qualities.GetInt(PLAYER_KILLER_STATUS_INT, 0) != RubberGlue_PKStatus)
	{
		m_MonsterAI = new MonsterAIManager(this, m_Position);
	}

	if (!_IsPlayer())
	{
		EmitEffect(PS_Create, 1.0f);
	}
}

CWeenieObject *CMonsterWeenie::SpawnWielded(DWORD wcid, int ptid, float shade)
{
	CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(wcid, NULL, false);

	if (!weenie)
	{
		return NULL;
	}

	if (!weenie->IsEquippable())
	{
		LOG_PRIVATE(Temp, Warning, "Trying to SpawnWielded an unwieldable item %u\n", wcid);
		delete weenie;

		return NULL;
	}

	weenie->SetID(g_pWorld->GenerateGUID(eDynamicGUID));

	if (!g_pWorld->CreateEntity(weenie, false))
		return NULL;
		
	if (ptid)
	{
		weenie->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, ptid);

		if (shade >= 0.0)
		{
			weenie->m_Qualities.SetFloat(SHADE_FLOAT, shade);
		}
	}

	if (!TryEquipItem(weenie, weenie->m_Qualities.GetInt(LOCATIONS_INT, 0)))
	{
		g_pWorld->RemoveEntity(weenie);
		weenie = NULL;
	}

	return weenie;
}


void CMonsterWeenie::Tick()
{
	CContainerWeenie::Tick();

	Movement_Think();

	if (m_MonsterAI)
		m_MonsterAI->Update();
}

void CMonsterWeenie::OnTookDamage(DamageEventData &damageData)
{
	CWeenieObject::OnTookDamage(damageData);

	//if (IsDead())
	//	return;

	if (m_MonsterAI)
		m_MonsterAI->OnTookDamage(damageData.source, damageData.outputDamageFinal);
}

/*
CBaelZharon::CBaelZharon()
{
	SetName("Bael'Zharon");
	SetSetupID(0x0200099E);
	SetScale(1.8f);

	m_ObjDesc.paletteID = 0x04001071;
	m_ObjDesc.AddSubpalette(new Subpalette(0x04001072, 0, 0));

	m_fTickFrequency = -1.0f;
}
*/

#if 0
CTargetDrudge::CTargetDrudge()
{
	SetSetupID(0x02000034);
	SetScale(0.95f);
	SetName("Oak Target Drudge");
	SetMotionTableID(0x0900008A);
	SetPETableID(0x3400006B);
	SetSoundTableID(0x20000051);

	m_miBaseModel.SetBasePalette(0x01B9);
	m_miBaseModel.ReplacePalette(0x08B4, 0x00, 0x00);
	m_miBaseModel.ReplaceTexture(0x00, 0x0036, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x01, 0x0031, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x02, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x03, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x04, 0x0D33, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x05, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x06, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x07, 0x0D33, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x08, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x09, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x0A, 0x0035, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x0B, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x0C, 0x0030, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x0D, 0x0035, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x0E, 0x0D33, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x0E, 0x0EE8, 0x0EE8);
	m_miBaseModel.ReplaceTexture(0x0F, 0x0035, 0x0D33);
	m_miBaseModel.ReplaceTexture(0x10, 0x0035, 0x0D33);
	m_miBaseModel.ReplaceModel(0x00, 0x005D);
	m_miBaseModel.ReplaceModel(0x01, 0x005E);
	m_miBaseModel.ReplaceModel(0x02, 0x006E);
	m_miBaseModel.ReplaceModel(0x03, 0x0064);
	m_miBaseModel.ReplaceModel(0x04, 0x18D9);
	m_miBaseModel.ReplaceModel(0x05, 0x006F);
	m_miBaseModel.ReplaceModel(0x06, 0x0316);
	m_miBaseModel.ReplaceModel(0x07, 0x18D9);
	m_miBaseModel.ReplaceModel(0x08, 0x006D);
	m_miBaseModel.ReplaceModel(0x09, 0x006B);
	m_miBaseModel.ReplaceModel(0x0A, 0x005F);
	m_miBaseModel.ReplaceModel(0x0B, 0x006C);
	m_miBaseModel.ReplaceModel(0x0C, 0x0068);
	m_miBaseModel.ReplaceModel(0x0D, 0x0060);
	m_miBaseModel.ReplaceModel(0x0E, 0x18D7);
	m_miBaseModel.ReplaceModel(0x0F, 0x0067);
	m_miBaseModel.ReplaceModel(0x10, 0x0060);
}
#endif

bool CMonsterWeenie::IsAttackMotion(DWORD motion)
{
	switch ((WORD)motion)
	{
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x69:
	case 0x6A:
		return true;
	}

	return false;
}

void CMonsterWeenie::OnMotionDone(DWORD motion, BOOL success)
{
	CWeenieObject::OnMotionDone(motion, success);

	if (motion == 0x40000011)
	{
		OnDeathAnimComplete();
	}

	if (m_MotionUseData.m_MotionUseType != MUT_UNDEF)
	{
		bool bUseSuccess = success && !IsDead();

		if (motion == m_MotionUseData.m_MotionUseMotionID)
		{
			switch (m_MotionUseData.m_MotionUseType)
			{
			case MUT_CONSUME_FOOD:
				{
					bool bConsumed = false;
					MotionUseData useData = m_MotionUseData;

					if (bUseSuccess)
					{
						m_MotionUseData.Reset(); // necessary so this doesn't become infinitely recursive

						DoForcedStopCompletely();

						CWeenieObject *pItem = FindContainedItem(useData.m_MotionUseTarget);

						if (pItem)
						{
							ReleaseContainedItemRecursive(pItem);

							bConsumed = true;

							if (DWORD use_sound_did = pItem->InqDIDQuality(USE_SOUND_DID, 0))
								EmitSound(use_sound_did, 1.0f);

							DWORD boost_stat = pItem->InqIntQuality(BOOSTER_ENUM_INT, 0);
							DWORD boost_value = pItem->InqIntQuality(BOOST_VALUE_INT, 0);

							switch (boost_stat)
							{
							case HEALTH_ATTRIBUTE_2ND:
							case STAMINA_ATTRIBUTE_2ND:
							case MANA_ATTRIBUTE_2ND:
								{
									STypeAttribute2nd maxStatType = (STypeAttribute2nd)(boost_stat - 1);
									STypeAttribute2nd statType = (STypeAttribute2nd)boost_stat;

									DWORD statValue = 0, maxStatValue = 0;
									m_Qualities.InqAttribute2nd(statType, statValue, FALSE);
									m_Qualities.InqAttribute2nd(maxStatType, maxStatValue, FALSE);

									DWORD newStatValue = min(statValue + boost_value, maxStatValue);

									int statChange = newStatValue - statValue;
									if (statChange)
									{
										m_Qualities.SetAttribute2nd(statType, newStatValue);
										NotifyAttribute2ndStatUpdated(statType);
									}

									const char *vitalName = "";
									switch (boost_stat)
									{
									case HEALTH_ATTRIBUTE_2ND: vitalName = "health"; break;
									case STAMINA_ATTRIBUTE_2ND: vitalName = "stamina"; break;
									case MANA_ATTRIBUTE_2ND: vitalName = "mana"; break;
									}

									SendText(csprintf("The %s restores %d points of your %s.", pItem->GetName().c_str(), max(0, statChange), vitalName), LTT_DEFAULT);
									break;
								}
							}

							BinaryWriter removeInventoryObjectMessage;
							removeInventoryObjectMessage.Write<DWORD>(0x24);
							removeInventoryObjectMessage.Write<DWORD>(pItem->id);
							SendNetMessage(&removeInventoryObjectMessage, PRIVATE_MSG, FALSE, FALSE);

							pItem->MarkForDestroy();
						}
					}

					if (!bConsumed)
						NotifyInventoryFailedEvent(useData.m_MotionUseTarget, 0);

					break;
				}
			}

			m_MotionUseData.Reset();
		}
	}
}

CCorpseWeenie *CMonsterWeenie::CreateCorpse()
{
	if (!InValidCell())
		return NULL;

	// spawn corpse
	CCorpseWeenie *pCorpse = (CCorpseWeenie *) g_pWeenieFactory->CreateWeenieByClassID(W_CORPSE_CLASS);

	pCorpse->CopyDIDStat(SETUP_DID, this);
	pCorpse->CopyDIDStat(MOTION_TABLE_DID, this);
	// pCorpse->CopyDIDStat(SOUND_TABLE_DID, this);
	// pCorpse->CopyDIDStat(PHYSICS_EFFECT_TABLE_DID, this);
	pCorpse->CopyFloatStat(DEFAULT_SCALE_FLOAT, this);
	pCorpse->CopyFloatStat(TRANSLUCENCY_FLOAT, this);

	ObjDesc desc;
	GetObjDesc(desc);
	pCorpse->SetObjDesc(desc);

	pCorpse->SetInitialPosition(m_Position);
	pCorpse->SetName(csprintf("Corpse of %s", GetName().c_str()));
	pCorpse->InitPhysicsObj();

	pCorpse->m_bDontClear = false;

	pCorpse->m_Qualities.SetString(LONG_DESC_STRING, csprintf("Killed by %s.", m_DeathKillerNameForCorpse.empty() ? "a mysterious source" : m_DeathKillerNameForCorpse.c_str()));
	pCorpse->m_Qualities.SetInstanceID(KILLER_IID, m_DeathKillerIDForCorpse);
	pCorpse->m_Qualities.SetInstanceID(VICTIM_IID, GetID());

	pCorpse->MakeMovementManager(TRUE);

	MovementParameters params;
	params.autonomous = 0;
	pCorpse->last_move_was_autonomous = false;
	pCorpse->DoMotion(GetCommandID(17), &params, 0);

	if (!g_pWorld->CreateEntity(pCorpse))
		pCorpse = NULL;

	m_DeathKillerIDForCorpse = 0;
	m_DeathKillerNameForCorpse.clear();

	return pCorpse;
}

void CMonsterWeenie::DropAllLoot(CCorpseWeenie *pCorpse)
{
}

void CMonsterWeenie::GenerateDeathLoot(CCorpseWeenie *pCorpse)
{
	if (m_Qualities._create_list)
	{
		for (auto i = m_Qualities._create_list->begin(); i != m_Qualities._create_list->end(); i++)
		{
			if (!(i->destination & DestinationType::Contain_DestinationType) &&
				!(i->destination & DestinationType::Treasure_DestinationType))
				continue;
			
			if (i->probability > 0 && (i->destination & DestinationType::Treasure_DestinationType))
			{
				float dice = Random::RollDice(0.0f, 1.0f) * g_pConfig->DropRateMultiplier();
				
				if (dice > i->probability)
					continue; // failed dice roll
			}

			if (!i->wcid)
				continue;

			CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(i->wcid, NULL, false);

			if (weenie)
			{
				if (weenie->IsDestroyedOnDeath())
				{
					delete weenie;
				}
				else if (g_pWorld->CreateEntity(weenie))
				{
					if (i->palette)
						weenie->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, i->palette);

					if (i->shade >= 0.0)
						weenie->m_Qualities.SetFloat(SHADE_FLOAT, i->shade);

					if (i->amount > 1 && weenie->InqIntQuality(MAX_STACK_SIZE_INT, 1) >= i->amount)
					{
						weenie->SetStackSize(i->amount);
					}

					weenie->SetWeenieContainer(pCorpse->GetID());
					pCorpse->Container_InsertInventoryItem(0, weenie, 0);
				}
			}
		}
	}

	if (DWORD death_treasure_did = InqDIDQuality(DEATH_TREASURE_TYPE_DID, 0))
	{
		if (int lootTier = g_pPortalDataEx->_treasureTableData.GetLootTierForTreasureEntry(death_treasure_did))
		{
			float numItemsRoll = Random::RollDice(0.0f, 1.0f);

			int numItems = 0;			
			if (numItemsRoll < 0.5)
			{
				numItems++;
				if (numItemsRoll < 0.2)
				{
					numItems++;
					if (numItemsRoll < 0.1)
					{
						numItems++;
						if (numItemsRoll < 0.05)
						{
							numItems++;
						}
					}
				}
			}

			for (int i = 0; i < numItems; i++)
			{
				if (CWeenieObject *weenie = g_pTreasureFactory->RollLootForTier(lootTier))
				{
					g_pWorld->CreateEntity(weenie);

					weenie->SetWeenieContainer(pCorpse->GetID());
					pCorpse->Container_InsertInventoryItem(0, weenie, 0);
				}				
			}
		}
	}
}

void CMonsterWeenie::OnDeathAnimComplete()
{
	if (!m_bWaitingForDeathToFinish)
		return;

	m_bWaitingForDeathToFinish = false;

	if (!_IsPlayer())
	{
		MarkForDestroy();
	}

	// create corpse
	CCorpseWeenie *pCorpse = CreateCorpse();

	if (pCorpse && !_IsPlayer())
	{
		GenerateDeathLoot(pCorpse);
	}
}

void CMonsterWeenie::OnDeath(DWORD killer_id)
{
	CWeenieObject::OnDeath(killer_id);

	m_DeathKillerIDForCorpse = killer_id;
	if (!g_pWorld->FindObjectName(killer_id, m_DeathKillerNameForCorpse))
		m_DeathKillerNameForCorpse.clear();

	MakeMovementManager(TRUE);
	StopCompletely(0);

	bool bHardcoreDeath = false;

	if (g_pConfig->HardcoreMode() && _IsPlayer())
	{
		if (CWeenieObject *pKiller = g_pWorld->FindObject(killer_id))
		{
			if (!g_pConfig->HardcoreModePlayersOnly() || pKiller->_IsPlayer())
			{
				bHardcoreDeath = true;
			}
		}
	}

	if (!bHardcoreDeath)
	{
		MovementStruct mvs;
		MovementParameters params;

		mvs.type = RawCommand;
		mvs.motion = Motion_Dead;
		mvs.params = &params;
		params.action_stamp = ++m_wAnimSequence;
		last_move_was_autonomous = 0;

		m_bWaitingForDeathToFinish = true;
		if (movement_manager->PerformMovement(mvs))
		{
			// animation failed for some reason
			OnDeathAnimComplete();
		}
		else
		{
			Animation_Update();
		}
	}
	else
	{
		if (!m_DeathKillerNameForCorpse.empty())
		{
			g_pWorld->BroadcastGlobal(ServerText(csprintf("%s has been defeated by %s.", GetName().c_str(), m_DeathKillerNameForCorpse.c_str()), LTT_DEFAULT), PRIVATE_MSG);
		}
		else
		{
			g_pWorld->BroadcastGlobal(ServerText(csprintf("%s has been defeated.", GetName().c_str()), LTT_DEFAULT), PRIVATE_MSG);
		}

		if (CCorpseWeenie *pCorpse = CreateCorpse())
		{
			DropAllLoot(pCorpse);
		}

		g_pDBIO->DeleteCharacter(GetID());

		BeginLogout();
	}
}

bool CMonsterWeenie::IsDead()
{
	if (m_bReviveAfterAnim)
		return true;

	return CWeenieObject::IsDead();
}

bool GetEquipPlacementAndHoldLocation(CWeenieObject *item, DWORD location, DWORD *pPlacementFrame, DWORD *pHoldLocation)
{
	if (location & MELEE_WEAPON_LOC)
	{
		*pPlacementFrame = Placement::RightHandCombat;
		*pHoldLocation = PARENT_RIGHT_HAND;
	}
	else if (location & MISSILE_WEAPON_LOC)
	{
		*pPlacementFrame = Placement::LeftHand;
		*pHoldLocation = PARENT_LEFT_HAND;
	}
	else if (location & SHIELD_LOC)
	{
		*pPlacementFrame = Placement::Shield;
		*pHoldLocation = PARENT_SHIELD;
	}
	else if (location & HELD_LOC)
	{
		*pPlacementFrame = Placement::RightHandCombat;
		*pHoldLocation = PARENT_RIGHT_HAND;
	}
	else if (location & MISSILE_AMMO_LOC)
	{
		/*
		*pPlacementFrame = Placement::RightHandCombat;
		*pHoldLocation = PARENT_RIGHT_HAND;
		*/
		*pPlacementFrame = 0;
		*pHoldLocation = 0;
	}
	else if (location & (ARMOR_LOC|CLOTHING_LOC))
	{
		*pPlacementFrame = 0;
		*pHoldLocation = 0;
		return false;
	}
	else
	{
		// LOG(Temp, Normal, "Trying to place %s but don't know how with coverage 0x%08X\n", item->GetName().c_str(), location);
		*pPlacementFrame = 0;
		*pHoldLocation = 0;
		return false;
	}

	return true;
}

bool CMonsterWeenie::TryEquipItem(DWORD item_id, DWORD inv_location)
{
	CWeenieObject *item_weenie = g_pWorld->FindWithinPVS(this, item_id);
	if (!item_weenie)
		return false;

	return TryEquipItem(item_weenie, inv_location);
}

BYTE GetEnchantmentSerialByteForMask(int priority)
{
	if (!priority)
		return 0;

	for (DWORD i = 0; i < 32; i++)
	{
		if (priority & 1)
			return i + 1;

		priority >>= 1;
	}

	return 0;
}

bool CheckWieldRequirements(CWeenieObject *item, CWeenieObject *wielder, STypeInt requirementStat, STypeInt skillStat, STypeInt difficultyStat)
{
	int requirementType = item->InqIntQuality(requirementStat, 0, TRUE);

	if (!requirementType)
	{
		return true;
	}

	int skillType = item->InqIntQuality(skillStat, 0, TRUE);
	int wieldDifficulty = item->InqIntQuality(difficultyStat, 0, TRUE);

	switch (requirementType)
	{
	case 1: // skill
		{
			DWORD skillLevel = 0;
			if (!wielder->m_Qualities.InqSkill((STypeSkill)skillType, skillLevel, FALSE) || (int)skillLevel < wieldDifficulty)
			{
				return false;
			}		
			return true;
		}

	case 8: // skill
		{
			SKILL_ADVANCEMENT_CLASS sac = SKILL_ADVANCEMENT_CLASS::UNDEF_SKILL_ADVANCEMENT_CLASS;
			if (!wielder->m_Qualities.InqSkillAdvancementClass((STypeSkill)skillType, sac) || (int)sac < wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 2: // base skill
		{
			DWORD skillLevel = 0;
			if (!wielder->m_Qualities.InqSkill((STypeSkill)skillType, skillLevel, TRUE) || (int)skillLevel < wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 3: // attribute
		{
			DWORD skillLevel = 0;
			if (!wielder->m_Qualities.InqAttribute((STypeAttribute)skillType, skillLevel, FALSE) || (int)skillLevel < wieldDifficulty)
			{
				return false;
			}		
			return true;
		}

	case 4: // base attribute
		{
			DWORD skillLevel = 0;
			if (!wielder->m_Qualities.InqAttribute((STypeAttribute)skillType, skillLevel, TRUE) || (int)skillLevel < wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 5: // attribute 2nd 
		{
			DWORD skillLevel = 0;
			if (!wielder->m_Qualities.InqAttribute2nd((STypeAttribute2nd)skillType, skillLevel, FALSE) || (int)skillLevel < wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 6: // attribute 2nd base
		{
			DWORD skillLevel = 0;
			if (!wielder->m_Qualities.InqAttribute2nd((STypeAttribute2nd)skillType, skillLevel, TRUE) || (int)skillLevel < wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 7: // level
		{
			if (wielder->InqIntQuality(LEVEL_INT, 1, TRUE) < wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 11: // type
		{
			if (wielder->InqIntQuality(CREATURE_TYPE_INT, 0, TRUE) != wieldDifficulty)
			{
				return false;
			}
			return true;
		}

	case 12: // race
		{
			if (wielder->InqIntQuality(HERITAGE_GROUP_INT, 0, TRUE) != wieldDifficulty)
			{
				return false;
			}
			return true;
		}
	}

	return true;
}

bool CMonsterWeenie::TryEquipItem(CWeenieObject *item_weenie, DWORD inv_location)
{
	if (!item_weenie || !inv_location)
		return false;

	// Scenarios to consider:
	// 1. Item being equipped from the GROUND!
	// 2. Item being equipped from different equip slot.
	// 3. Item being equipped from the player's inventory.

	// Can this item be equipped?
	if (item_weenie->IsStuck())
		return false;

	if (!Container_CanEquip(item_weenie, inv_location))
		return false;

	if (!CheckWieldRequirements(item_weenie, this, WIELD_REQUIREMENTS_INT, WIELD_SKILLTYPE_INT, WIELD_DIFFICULTY_INT))
		return false;
	if (!CheckWieldRequirements(item_weenie, this, WIELD_REQUIREMENTS_2_INT, WIELD_SKILLTYPE_2_INT, WIELD_DIFFICULTY_2_INT))
		return false;
	if (!CheckWieldRequirements(item_weenie, this, WIELD_REQUIREMENTS_3_INT, WIELD_SKILLTYPE_3_INT, WIELD_DIFFICULTY_3_INT))
		return false;
	if (!CheckWieldRequirements(item_weenie, this, WIELD_REQUIREMENTS_4_INT, WIELD_SKILLTYPE_4_INT, WIELD_DIFFICULTY_4_INT))
		return false;

	if (!item_weenie->HasOwner())
	{
		if (CWeenieObject *generator = g_pWorld->FindObject(item_weenie->InqIIDQuality(GENERATOR_IID, 0)))
		{
			generator->NotifyGeneratedPickedUp(item_weenie->GetID());
		}
	}

	DWORD cell_id = m_Position.objcell_id;

	// Take it out of whatever slot it may be in
	item_weenie->ReleaseFromAnyWeenieParent(false, false);

	DWORD placement_id, child_location_id;
	bool bShouldPlace = GetEquipPlacementAndHoldLocation(item_weenie, inv_location, &placement_id, &child_location_id);

	item_weenie->SetWielderID(GetID());
	item_weenie->SetWieldedLocation(inv_location);
	item_weenie->ReleaseFromBlock();

	// The container will auto-correct this slot into a valid range.
	Container_EquipItem(cell_id, item_weenie, inv_location, child_location_id, placement_id);

	if (_IsPlayer())
	{
		SendNetMessage(InventoryEquip(item_weenie->GetID(), inv_location), PRIVATE_MSG, TRUE);
		EmitSound(Sound_WieldObject, 1.0f);
	}

	if (m_bWorldIsAware)
	{
		item_weenie->NotifyIIDStatUpdated(CONTAINER_IID, false);
		item_weenie->NotifyIIDStatUpdated(WIELDER_IID, false);
		item_weenie->NotifyIntStatUpdated(CURRENT_WIELDED_LOCATION_INT, false);
	}

	if (item_weenie->AsClothing() && m_bWorldIsAware)
	{
		UpdateModel();
	}

	item_weenie->m_Qualities.RemoveFloat(TIME_TO_ROT_FLOAT);
	item_weenie->_timeToRot = -1.0;
	item_weenie->_beganRot = false;

	if (get_minterp()->InqStyle() != Motion_NonCombat)
	{
		AdjustToNewCombatMode();
	}

	// apply enchantments
	if (item_weenie->m_Qualities._spell_book)
	{
		bool bShouldCast = true;

		int difficulty;
		difficulty = 0;
		if (item_weenie->m_Qualities.InqInt(ITEM_DIFFICULTY_INT, difficulty, TRUE, FALSE))
		{
			DWORD skillLevel = 0;
			if (!m_Qualities.InqSkill(ARCANE_LORE_SKILL, skillLevel, FALSE) || (int)skillLevel < difficulty)
			{
				bShouldCast = false;

				NotifyWeenieError(WERROR_ACTIVATION_ARCANE_LORE_TOO_LOW);
			}
		}

		if (bShouldCast)
		{
			difficulty = 0;
			DWORD skillActivationTypeDID = 0;
			if (item_weenie->m_Qualities.InqInt(ITEM_SKILL_LEVEL_LIMIT_INT, difficulty, TRUE, FALSE) && item_weenie->m_Qualities.InqDataID(ITEM_SKILL_LIMIT_DID, skillActivationTypeDID))
			{
				STypeSkill skillActivationType = SkillTable::OldToNewSkill((STypeSkill)skillActivationTypeDID);

				DWORD skillLevel = 0;
				if (!m_Qualities.InqSkill(skillActivationType, skillLevel, FALSE) || (int)skillLevel < difficulty)
				{
					bShouldCast = false;

					NotifyWeenieErrorWithString(WERROR_ACTIVATION_SKILL_TOO_LOW, CachedSkillTable->GetSkillName(skillActivationType).c_str());
				}
			}
		}

		if (bShouldCast)
		{
			DWORD serial = 0;
		
			serial |= ((DWORD)GetEnchantmentSerialByteForMask(item_weenie->InqIntQuality(LOCATIONS_INT, 0, TRUE)) << (DWORD)0);
			serial |= ((DWORD)GetEnchantmentSerialByteForMask(item_weenie->InqIntQuality(CLOTHING_PRIORITY_INT, 0, TRUE)) << (DWORD)8);

			for (auto &spellPage : item_weenie->m_Qualities._spell_book->_spellbook)
			{
				item_weenie->MakeSpellcastingManager()->CastSpellEquipped(GetID(), spellPage.first, (WORD) serial);
			}
		}
	}

	return true;
}

void CMonsterWeenie::ChangeCombatMode(COMBAT_MODE mode, bool playerRequested)
{
	COMBAT_MODE newCombatMode = (COMBAT_MODE) InqIntQuality(COMBAT_MODE_INT, COMBAT_MODE::NONCOMBAT_COMBAT_MODE, TRUE);
	DWORD new_motion_style = get_minterp()->InqStyle();

	switch (mode)
	{
	case NONCOMBAT_COMBAT_MODE:
		new_motion_style = Motion_NonCombat;
		newCombatMode = COMBAT_MODE::NONCOMBAT_COMBAT_MODE;
		break;

	case MELEE_COMBAT_MODE:
		{
			CWeenieObject *weapon = GetWieldedMelee();

			CombatStyle default_combat_style = weapon ? (CombatStyle)weapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, Undef_CombatStyle) : Undef_CombatStyle;

			switch (default_combat_style)
			{
			case Undef_CombatStyle:
			case Unarmed_CombatStyle:
				new_motion_style = Motion_HandCombat;
				newCombatMode = COMBAT_MODE::MELEE_COMBAT_MODE;
				break;

			case OneHanded_CombatStyle:
				{
					new_motion_style = Motion_SwordCombat;

					if (CWeenieObject *shield = GetWieldedShield())
					{
						new_motion_style = Motion_SwordShieldCombat;
					}

					newCombatMode = COMBAT_MODE::MELEE_COMBAT_MODE;
					break;
				}
			}

			break;
		}

	case MISSILE_COMBAT_MODE:
		{
			CWeenieObject *weapon = GetWieldedMissile();
			CombatStyle default_combat_style = weapon ? (CombatStyle)weapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, Undef_CombatStyle) : Undef_CombatStyle;

			switch (default_combat_style)
			{
			case Bow_CombatStyle:
				{
					new_motion_style = Motion_BowCombat;
					newCombatMode = COMBAT_MODE::MISSILE_COMBAT_MODE;

					if (CWeenieObject *ammo = GetWieldedAmmo())
					{
					}
					else
					{
						// new_motion_style = Motion_BowNoAmmo;
						// newCombatMode = COMBAT_MODE::NONCOMBAT_COMBAT_MODE;
					}

					break;
				}

			case Crossbow_CombatStyle:
				{
					new_motion_style = Motion_CrossbowCombat;
					newCombatMode = COMBAT_MODE::MISSILE_COMBAT_MODE;

					if (CWeenieObject *ammo = GetWieldedAmmo())
					{
						new_motion_style = Motion_CrossbowCombat;
						newCombatMode = COMBAT_MODE::MISSILE_COMBAT_MODE;
					}
					else
					{
						// new_motion_style = Motion_NonCombat;
						// newCombatMode = COMBAT_MODE::NONCOMBAT_COMBAT_MODE;
					}

					break;
				}

			case ThrownWeapon_CombatStyle:
				{
					new_motion_style = Motion_ThrownWeaponCombat;
					newCombatMode = COMBAT_MODE::MISSILE_COMBAT_MODE;
					break;
				}
			}
			break;
		}

	case MAGIC_COMBAT_MODE:
		new_motion_style = Motion_Magic;
		newCombatMode = COMBAT_MODE::MAGIC_COMBAT_MODE;
		break;
	}

	if (new_motion_style != get_minterp()->InqStyle())
	{
		MovementParameters params;
		get_minterp()->DoMotion(new_motion_style, &params);

		_server_control_timestamp++;
		last_move_was_autonomous = false;

		Animation_Update();
	}

	m_Qualities.SetInt(COMBAT_MODE_INT, newCombatMode);

	if (newCombatMode != mode || !playerRequested)
	{
		NotifyIntStatUpdated(COMBAT_MODE_INT);
	}
}

DWORD CMonsterWeenie::DoForcedUseMotion(MotionUseType useType, DWORD motion, DWORD target, DWORD childID, DWORD childLoc, MovementParameters *params)
{
	m_MotionUseData.m_MotionUseType = useType;
	m_MotionUseData.m_MotionUseMotionID = motion;
	m_MotionUseData.m_MotionUseTarget = target;
	m_MotionUseData.m_MotionUseChildID = childID;
	m_MotionUseData.m_MotionUseChildLocation = childLoc;
	return DoForcedMotion(motion, params);
}

bool CMonsterWeenie::ClothingPrioritySorter(const CWeenieObject *first, const CWeenieObject *second)
{
	return ((CWeenieObject *)first)->InqIntQuality(CLOTHING_PRIORITY_INT, 0, TRUE) < ((CWeenieObject *)second)->InqIntQuality(CLOTHING_PRIORITY_INT, 0, TRUE);
}

void CMonsterWeenie::GetObjDesc(ObjDesc &objDesc)
{
	std::list<CWeenieObject *> wieldedWearable;
	Container_GetWieldedByMask(wieldedWearable, ARMOR_LOC|CLOTHING_LOC);
	for (auto wearable : wieldedWearable)
	{
		if (wearable->IsAvatarJumpsuit())
		{
			objDesc = wearable->m_WornObjDesc;
			return;
		}
	}

	CWeenieObject::GetObjDesc(objDesc);

	DWORD head_object_id;
	if (m_Qualities.InqDataID(HEAD_OBJECT_DID, head_object_id))
		objDesc.AddAnimPartChange(new AnimPartChange(16, head_object_id));

	DWORD old_eye_texture_id, new_eye_texture_id;
	if (m_Qualities.InqDataID(DEFAULT_EYES_TEXTURE_DID, old_eye_texture_id) && m_Qualities.InqDataID(EYES_TEXTURE_DID, new_eye_texture_id))
		objDesc.AddTextureMapChange(new TextureMapChange(16, old_eye_texture_id, new_eye_texture_id));

	DWORD old_nose_texture_id, new_nose_texture_id;
	if (m_Qualities.InqDataID(DEFAULT_NOSE_TEXTURE_DID, old_nose_texture_id) && m_Qualities.InqDataID(NOSE_TEXTURE_DID, new_nose_texture_id))
		objDesc.AddTextureMapChange(new TextureMapChange(16, old_nose_texture_id, new_nose_texture_id));

	DWORD old_mouth_texture_id, new_mouth_texture_id;
	if (m_Qualities.InqDataID(DEFAULT_MOUTH_TEXTURE_DID, old_mouth_texture_id) && m_Qualities.InqDataID(MOUTH_TEXTURE_DID, new_mouth_texture_id))
		objDesc.AddTextureMapChange(new TextureMapChange(16, old_mouth_texture_id, new_mouth_texture_id));

	DWORD skin_palette_id;
	if (m_Qualities.InqDataID(SKIN_PALETTE_DID, skin_palette_id))
		objDesc.AddSubpalette(new Subpalette(skin_palette_id, 0 << 3, 0x18 << 3));

	DWORD hair_palette_id;
	if (m_Qualities.InqDataID(HAIR_PALETTE_DID, hair_palette_id))
		objDesc.AddSubpalette(new Subpalette(hair_palette_id, 0x18 << 3, 0x8 << 3));

	DWORD eye_palette_id;
	if (m_Qualities.InqDataID(EYES_PALETTE_DID, eye_palette_id))
		objDesc.AddSubpalette(new Subpalette(eye_palette_id, 0x20 << 3, 0x8 << 3));

	/*
	std::list<CWeenieObject *> wieldedClothings;
	Container_GetWieldedByMask(wieldedClothings, CLOTHING_LOC);
	wieldedClothings.sort(ClothingPrioritySorter);

	for (auto clothing : wieldedClothings)
	{
		if (clothing->IsHelm())
		{
			if (!ShowHelm())
				continue;
		}
		
		DWORD clothing_table_id = clothing->InqDIDQuality(CLOTHINGBASE_DID, 0);
		DWORD palette_template_key = clothing->InqIntQuality(PALETTE_TEMPLATE_INT, 0);

		if (clothing_table_id && !clothing->IsAvatarJumpsuit())
		{
			ClothingTable *clothingTable = ClothingTable::Get(clothing_table_id);

			if (clothingTable)
			{
				ObjDesc od;
				ShadePackage shades(clothing->InqFloatQuality(SHADE_FLOAT, 0.0));

				double shadeVal;
				if (clothing->m_Qualities.InqFloat(SHADE2_FLOAT, shadeVal))
					shades._val[1] = shadeVal;
				if (clothing->m_Qualities.InqFloat(SHADE3_FLOAT, shadeVal))
					shades._val[2] = shadeVal;
				if (clothing->m_Qualities.InqFloat(SHADE4_FLOAT, shadeVal))
					shades._val[3] = shadeVal;

				clothingTable->BuildObjDesc(GetSetupID(), palette_template_key, &shades, &od);
				objDesc += od;

				ClothingTable::Release(clothingTable);
			}
		}
		else
		{
			objDesc += clothing->m_WornObjDesc;
		}
	}
	*/

	std::list<CWeenieObject *> wieldedArmors;
	Container_GetWieldedByMask(wieldedArmors, CLOTHING_LOC|ARMOR_LOC);
	wieldedArmors.sort(ClothingPrioritySorter);

	for (auto armor : wieldedArmors)
	{
		if (armor->IsHelm())
		{
			if (!ShowHelm())
				continue;
		}

		DWORD clothing_table_id = armor->InqDIDQuality(CLOTHINGBASE_DID, 0);
		DWORD palette_template_key = armor->InqIntQuality(PALETTE_TEMPLATE_INT, 0);

		if (clothing_table_id && !armor->IsAvatarJumpsuit())
		{
			ClothingTable *clothingTable = ClothingTable::Get(clothing_table_id);

			if (clothingTable)
			{
				ObjDesc od;
				ShadePackage shades(armor->InqFloatQuality(SHADE_FLOAT, 0.0));

				double shadeVal;
				if (armor->m_Qualities.InqFloat(SHADE2_FLOAT, shadeVal))
					shades._val[1] = shadeVal;
				if (armor->m_Qualities.InqFloat(SHADE3_FLOAT, shadeVal))
					shades._val[2] = shadeVal;
				if (armor->m_Qualities.InqFloat(SHADE4_FLOAT, shadeVal))
					shades._val[3] = shadeVal;

				clothingTable->BuildObjDesc(GetSetupID(), palette_template_key, &shades, &od);
				objDesc += od;

				ClothingTable::Release(clothingTable);
			}
		}
		else
		{
			objDesc += armor->m_WornObjDesc;
		}
	}
}

DWORD CMonsterWeenie::ReceiveInventoryItem(CWeenieObject *source, CWeenieObject *item, DWORD desired_slot)
{
	if (m_Qualities._emote_table)
	{
		PackableList<EmoteSet> *emoteCategory = m_Qualities._emote_table->_emote_table.lookup(Give_EmoteCategory);
		
		if (emoteCategory)
		{
			double dice = Random::GenFloat(0.0, 1.0);
			double lastProbability = -1.0;

			for (auto &emoteSet : *emoteCategory)
			{
				if (emoteSet.classID == item->m_Qualities.id)
				{
					if (dice >= emoteSet.probability)
						continue;

					if (lastProbability < 0.0 || lastProbability == emoteSet.probability)
					{
						MakeEmoteManager()->ExecuteEmoteSet(emoteSet, source->GetID());

						lastProbability = emoteSet.probability;
					}
				}
			}
		}
	}

	g_pWorld->RemoveEntity(item);

	return 0;
}

double CMonsterWeenie::GetDefenseMod()
{
	/*
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC|HELD_LOC);

	float mod = CWeenieObject::GetDefenseMod();

	for (auto item : wielded)
	{
		mod *= item->GetDefenseMod();
	}

	return mod;
	*/
	return CWeenieObject::GetDefenseMod();
}

double CMonsterWeenie::GetDefenseModUsingWielded()
{
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC | HELD_LOC);

	for (auto item : wielded)
	{
		// use first item, sigh, not correct
		return item->GetDefenseMod();
	}

	return CWeenieObject::GetDefenseMod();
}

double CMonsterWeenie::GetOffenseMod()
{
	/*
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC | HELD_LOC);

	float mod = CWeenieObject::GetOffenseMod();

	for (auto item : wielded)
	{
		mod *= item->GetOffenseMod();
	}

	return mod;
	*/
	return CWeenieObject::GetOffenseMod();
}

int CMonsterWeenie::GetAttackTimeUsingWielded()
{
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC);
	
	for (auto item : wielded)
	{
		return item->GetAttackTime();
	}

	return CWeenieObject::GetAttackTime();
}

int CMonsterWeenie::GetAttackTime()
{
	/*
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC);

	int totalTime = CWeenieObject::GetAttackTime();

	for (auto item : wielded)
	{
		totalTime += item->GetAttackTime();
	}

	return totalTime;
	*/
	return CWeenieObject::GetAttackTime();
}

int CMonsterWeenie::GetAttackDamage()
{
	/*
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC);

	int damage = CWeenieObject::GetAttackDamage();

	for (auto item : wielded)
	{
		damage += item->GetAttackDamage();
	}
	*/

	return CWeenieObject::GetAttackDamage();
}

float CMonsterWeenie::GetEffectiveArmorLevel(DamageEventData &damageData, bool bIgnoreMagicArmor)
{
	std::list<CWeenieObject *> wielded;
	Container_GetWieldedByMask(wielded, ARMOR_LOC|CLOTHING_LOC|SHIELD_LOC);

	// use body part
	float armorLevel = CWeenieObject::GetEffectiveArmorLevel(damageData, bIgnoreMagicArmor);

	/*
	if (m_Qualities._body)
	{
		BodyPart *part = m_Qualities._body->_body_part_table.lookup(damageData.hitPart);

		if (part)
		{	
			int baseArmor = 0;
			m_Qualities.InqBodyArmorValue(damageData.hitPart, damageData.damage_type, baseArmor, bIgnoreMagicArmor);
			armorLevel += baseArmor;
		}
	}
	*/
	int baseArmor = 0;
	m_Qualities.InqBodyArmorValue(damageData.hitPart, damageData.damage_type, baseArmor, bIgnoreMagicArmor);
	armorLevel += baseArmor;

	for (auto item : wielded)
	{
		armorLevel += item->GetEffectiveArmorLevel(damageData, bIgnoreMagicArmor);
	}

	return armorLevel;
}

void CMonsterWeenie::OnIdentifyAttempted(CWeenieObject *other)
{
	if (m_MonsterAI)
		m_MonsterAI->OnIdentifyAttempted(other);
}

void CMonsterWeenie::HandleAggro(CWeenieObject *attacker)
{
	if (m_MonsterAI)
		m_MonsterAI->HandleAggro(attacker);
}

void CMonsterWeenie::ResistedSpell(CWeenieObject *attacker)
{
	HandleAggro(attacker);
}

void CMonsterWeenie::EvadedAttack(CWeenieObject *attacker)
{
	HandleAggro(attacker);
}

void CMonsterWeenie::TryMeleeAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion)
{
	if (IsDead())
	{
		NotifyAttackDone(WERROR_DEAD);
		return;
	}

	if (!IsAttacking() && IsBusyOrInAction())
	{
		NotifyAttackDone(WERROR_ACTIONS_LOCKED);
		return;
	}

	if (!m_AttackManager)
	{
		m_AttackManager = new AttackManager(this);
	}

	// duplicate attack
	m_AttackManager->BeginMeleeAttack(target_id, height, power, m_MonsterAI ? m_MonsterAI->GetChaseDistance() : 15.0f, motion);

	// ensure there's no heartbeat animation
	_last_update_pos = Timer::cur_time;
}

void CMonsterWeenie::TryMissileAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion)
{
	if (IsDead())
	{
		NotifyWeenieError(WERROR_DEAD);
		NotifyAttackDone();
		return;
	}

	if (!IsAttacking() && IsBusyOrInAction())
	{
		NotifyWeenieError(WERROR_ACTIONS_LOCKED);
		NotifyAttackDone();
		return;
	}

	if (!m_AttackManager)
		m_AttackManager = new AttackManager(this);

	// duplicate attack
	m_AttackManager->BeginMissileAttack(target_id, height, power, motion);

	// ensure there's no heartbeat animation
	_last_update_pos = Timer::cur_time;
}

BOOL CMonsterWeenie::DoCollision(const class ObjCollisionProfile &prof)
{
	if (prof.IsDoor() && !AsPlayer())
	{
		if (CWeenieObject *weenie = g_pWorld->FindObject(prof.id))
		{
			if (CBaseDoor *door = weenie->AsDoor())
			{
				if (!door->IsLocked() && door->IsClosed())
				{
					door->OpenDoor();
				}
			}
		}
	}

	return CContainerWeenie::DoCollision(prof);
}