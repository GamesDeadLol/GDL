
#include "StdAfx.h"
#include "WeenieObject.h"
#include "PhysicsObj.h"
#include "Monster.h"
#include "Player.h"
#include "Client.h"
#include "ClientEvents.h"
#include "BinaryWriter.h"
#include "ObjectMsgs.h"
#include "Database.h"
#include "Database2.h"
#include "World.h"
#include "WorldLandBlock.h"
#include "ClientCommands.h"
#include "ChatMsgs.h"
#include "WeenieFactory.h"
#include "InferredPortalData.h"
#include "AllegianceManager.h"
#include "Config.h"

#define PLAYER_SAVE_INTERVAL 180.0

CPlayerWeenie::CPlayerWeenie(CClient *pClient, DWORD dwGUID, WORD instance_ts)
{
	m_bDontClear = true;
	m_pClient = pClient;
	SetID(dwGUID);

	_instance_timestamp = instance_ts;

	m_Qualities.SetInt(CREATION_TIMESTAMP_INT, (int)time(NULL));
	m_Qualities.SetFloat(CREATION_TIMESTAMP_FLOAT, Timer::cur_time);

	//if (pClient && pClient->GetAccessLevel() >= SENTINEL_ACCESS)
	//	SetRadarBlipColor(Sentinel_RadarBlipEnum);

	SetLoginPlayerQualities();

	m_Qualities.SetInt(PHYSICS_STATE_INT, PhysicsState::HIDDEN_PS | PhysicsState::IGNORE_COLLISIONS_PS | PhysicsState::EDGE_SLIDE_PS | PhysicsState::GRAVITY_PS);

	// Human Female by Default

	// Used by physics object
	SetSetupID(0x2000001); // 0x0200004E);
	SetMotionTableID(0x9000001);
	SetSoundTableID(0x20000001); // 0x20000002);
	SetPETableID(0x34000004);
	SetScale(1.0f);

	// Moon location... loc_t( 0xEFEA0001, 0, 0, 0 );
	// SetInitialPosition(Position(0x9722003A, Vector(168.354004f, 24.618000f, 102.005005f), Quaternion(-0.922790f, 0.000000, 0.000000, -0.385302f)));

	m_LastAssessed = 0;

	m_NextSave = Timer::cur_time + PLAYER_SAVE_INTERVAL;
}

CPlayerWeenie::~CPlayerWeenie()
{
	LeaveFellowship();

	CClientEvents *pEvents;
	if (m_pClient && (pEvents = m_pClient->GetEvents()))
	{
		pEvents->DetachPlayer();
	}
}

void CPlayerWeenie::DetachClient()
{
	m_pClient = NULL;
}

void CPlayerWeenie::SendNetMessage(void *_data, DWORD _len, WORD _group, BOOL _event)
{
	if (m_pClient)
		m_pClient->SendNetMessage(_data, _len, _group, _event);
}

void CPlayerWeenie::SendNetMessage(BinaryWriter *_food, WORD _group, BOOL _event, BOOL del)
{
	if (m_pClient)
		m_pClient->SendNetMessage(_food, _group, _event, del);
}

void CPlayerWeenie::AddSpellByID(DWORD id)
{
	BinaryWriter AddSpellToSpellbook;
	AddSpellToSpellbook.Write<DWORD>(0x02C1);
	AddSpellToSpellbook.Write<DWORD>(id);
	AddSpellToSpellbook.Write<DWORD>(0x0);
	SendNetMessage(AddSpellToSpellbook.GetData(), AddSpellToSpellbook.GetSize(), EVENT_MSG, true);
}

bool CPlayerWeenie::IsAdvocate()
{
	return GetAccessLevel() >= ADVOCATE_ACCESS;
}

bool CPlayerWeenie::IsSentinel()
{
	return GetAccessLevel() >= SENTINEL_ACCESS;
}

bool CPlayerWeenie::IsAdmin()
{
	return GetAccessLevel() >= ADMIN_ACCESS;
}

int CPlayerWeenie::GetAccessLevel()
{
	if (!m_pClient)
		return BASIC_ACCESS;
	
	return m_pClient->GetAccessLevel();
}

void CPlayerWeenie::BeginLogout()
{
	if (IsLoggingOut())
		return;

	_logoutTime = Timer::cur_time + 5.0;

	LeaveFellowship();
	StopCompletely(0);
	DoForcedMotion(Motion_LogOut);
	Save();
}

void CPlayerWeenie::Tick()
{
	CMonsterWeenie::Tick();

	if (IsDead() && (!m_bReviveAfterAnim || get_minterp()->interpreted_state.forward_command != Motion_Dead))
	{
		m_bReviveAfterAnim = false;

		//if (IsDead())
		Revive();
	}

	double pkTimestamp;
	if (m_Qualities.InqFloat(PK_TIMESTAMP_FLOAT, pkTimestamp, TRUE))
	{
		if (pkTimestamp <= Timer::cur_time)
		{
			m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PKStatusEnum::PK_PKStatus);
			NotifyIntStatUpdated(PLAYER_KILLER_STATUS_INT, false);

			m_Qualities.RemoveFloat(PK_TIMESTAMP_FLOAT);
			NotifyFloatStatUpdated(PK_TIMESTAMP_FLOAT);

			SendText("The power of Bael'Zharon flows through you, you are once more a player killer.", LTT_MAGIC);
		}
	}

	if (m_pClient)
	{
		if (m_NextSave <= Timer::cur_time)
		{
			if (!IsDead()) // && !IsBusyOrInAction())
			{
				Save();
				m_NextSave = Timer::cur_time + PLAYER_SAVE_INTERVAL;
			}
		}
	}

	if (m_fNextMakeAwareCacheFlush <= Timer::cur_time)
	{
		FlushMadeAwareof();
		m_fNextMakeAwareCacheFlush = Timer::cur_time + 60.0;
	}

	if (IsLoggingOut() && _logoutTime <= Timer::cur_time)
	{
		// time to logout
		if (m_pClient && m_pClient->GetEvents())
		{
			m_pClient->GetEvents()->OnLogoutCompleted();
		}

		MarkForDestroy();
	}

	if (IsRecalling() && _recallTime <= Timer::cur_time)
	{
		_recallTime = -1.0;
		Movement_Teleport(_recallPos, false);
	}
}

bool CPlayerWeenie::IsBusy()
{
	if (IsRecalling() || IsLoggingOut() || CWeenieObject::IsBusy())
		return true;

	return false;
}

const double AWARENESS_TIMEOUT = 20.0;

bool CPlayerWeenie::AlreadyMadeAwareOf(DWORD object_id)
{
	std::unordered_map<DWORD, double>::iterator entry = _objMadeAwareOf.find(object_id);

	if (entry != _objMadeAwareOf.end())
	{
		if ((entry->second + AWARENESS_TIMEOUT) > Timer::cur_time)
			return true;
	}

	return false;
}

void CPlayerWeenie::SetMadeAwareOf(DWORD object_id)
{
	_objMadeAwareOf[object_id] = Timer::cur_time;
}

void CPlayerWeenie::FlushMadeAwareof()
{
	for (std::unordered_map<DWORD, double>::iterator i = _objMadeAwareOf.begin(); i != _objMadeAwareOf.end();)
	{
		if ((i->second + AWARENESS_TIMEOUT) <= Timer::cur_time)
			i = _objMadeAwareOf.erase(i);
		else
			i++;
	}
}

void CPlayerWeenie::MakeAware(CWeenieObject *pEntity, bool bForceUpdate)
{
#ifndef PUBLIC_BUILD
	int vis;
	if (pEntity->m_Qualities.InqBool(VISIBILITY_BOOL, vis) && !m_bAdminVision)
	{
		return;
	}
#else
	int vis;
	if (pEntity->m_Qualities.InqBool(VISIBILITY_BOOL, vis))
	{
		return;
	}
#endif

	if (!bForceUpdate && AlreadyMadeAwareOf(pEntity->GetID()))
		return;

	SetMadeAwareOf(pEntity->GetID());

	BinaryWriter *CM = pEntity->CreateMessage();

	if (CM)
	{
		// LOG(Temp, Normal, "Sending object %X in cell %X\n", pEntity->GetID(), pEntity->m_Position.objcell_id);
		SendNetMessage(CM, OBJECT_MSG);
	}

	if (pEntity->children && pEntity->children->num_objects)
	{
		for (DWORD i = 0; i < pEntity->children->num_objects; i++)
		{
			BinaryWriter *CM = ((CWeenieObject *)pEntity->children->objects.array_data[i])->CreateMessage();
			if (CM)
				SendNetMessage(CM, OBJECT_MSG);
		}
	}

	if (pEntity == this)
	{
		// make aware of inventory too
		for (auto item : m_Wielded)
		{
			MakeAware(item);
		}

		for (auto item : m_Items)
		{
			MakeAware(item);
		}
		
		for (auto item : m_Packs)
		{
			MakeAware(item);

			if (CContainerWeenie *container = item->AsContainer())
			{
				container->MakeAwareViewContent(this);
			}
		}
	}
}

void CPlayerWeenie::LoginCharacter(void)
{
	DWORD SC[2];

	SC[0] = 0xF746;
	SC[1] = GetID();
	SendNetMessage(SC, sizeof(SC), 10);

	BinaryWriter *LC = ::LoginCharacter(this);
	SendNetMessage(LC->GetData(), LC->GetSize(), PRIVATE_MSG, TRUE);
	delete LC;
}

void CPlayerWeenie::ExitPortal()
{
	if (_phys_obj)
		_phys_obj->ExitPortal();
}

void CPlayerWeenie::UpdateEntity(CWeenieObject *pEntity)
{
	BinaryWriter *CO = pEntity->UpdateMessage();
	if (CO)
	{
		g_pWorld->BroadcastPVS(GetLandcell(), CO->GetData(), CO->GetSize(), OBJECT_MSG, 0);
		delete CO;
	}
}

void CPlayerWeenie::SetLastAssessed(DWORD guid)
{
	m_LastAssessed = guid;
}

std::string CPlayerWeenie::RemoveLastAssessed()
{
	if (m_LastAssessed != 0)
	{
		CWeenieObject *pObject = g_pWorld->FindWithinPVS(this, m_LastAssessed);

		if (pObject != NULL && !pObject->AsPlayer() && !pObject->m_bDontClear) {
			std::string name = pObject->GetName();
			pObject->MarkForDestroy();
			m_LastAssessed = 0;
			return name;
		}
	}

	return "";
}

void CPlayerWeenie::OnDeathAnimComplete()
{
	CMonsterWeenie::OnDeathAnimComplete();

	if (m_bReviveAfterAnim)
	{
		m_bReviveAfterAnim = false;

		if (!g_pConfig->HardcoreMode())
		{
			//if (IsDead())
			Revive();
		}
	}
}

void CPlayerWeenie::UpdateVitaePool(DWORD pool)
{
	m_Qualities.SetInt(VITAE_CP_POOL_INT, pool);
	NotifyIntStatUpdated(VITAE_CP_POOL_INT, true);
}

void CPlayerWeenie::ReduceVitae(float amount)
{
	Enchantment enchant;
	if (m_Qualities.InqVitae(&enchant))
	{
		enchant._smod.val -= amount;
		if (enchant._smod.val < 0.6f)
			enchant._smod.val = 0.6f;
		if (enchant._smod.val > 1.0f)
			enchant._smod.val = 1.0f;
	}
	else
	{
		enchant._id = Vitae_SpellID;
		enchant.m_SpellSetID = 0; // ???
		enchant._spell_category = 204; // Vitae_SpellCategory;
		enchant._power_level = 30;
		enchant._start_time = 0;
		enchant._duration = -1.0;
		enchant._caster = 0;
		enchant._degrade_modifier = 0;
		enchant._degrade_limit = 1;
		enchant._last_time_degraded = 0;
		enchant._smod.type = SecondAtt_EnchantmentType | Skill_EnchantmentType | MultipleStat_EnchantmentType | Multiplicative_EnchantmentType | Additive_Degrade_EnchantmentType | Vitae_EnchantmentType;
		enchant._smod.key = 0;
		enchant._smod.val = 1.0f - amount;
	}

	m_Qualities.UpdateEnchantment(&enchant);
	UpdateVitaeEnchantment();

	CheckVitalRanges();
}

void CPlayerWeenie::UpdateVitaeEnchantment()
{
	Enchantment enchant;
	if (m_Qualities.InqVitae(&enchant))
	{
		if (enchant._smod.val < 1.0f)
		{
			NotifyEnchantmentUpdated(&enchant);
		}
		else
		{
			PackableListWithJson<DWORD> expired;
			expired.push_back(666);

			if (m_Qualities._enchantment_reg)
			{
				m_Qualities._enchantment_reg->RemoveEnchantments(&expired);
			}

			BinaryWriter expireMessage;
			expireMessage.Write<DWORD>(0x2C5);
			expired.Pack(&expireMessage);
			SendNetMessage(&expireMessage, PRIVATE_MSG, TRUE, FALSE);
			EmitSound(Sound_SpellExpire, 1.0f, true);
		}
	}
}

void CPlayerWeenie::OnGivenXP(long long amount, bool allegianceXP)
{
	if (m_Qualities.GetVitaeValue() < 1.0 && !allegianceXP)
	{
		DWORD64 vitae_pool = InqIntQuality(VITAE_CP_POOL_INT, 0) + min(amount, 1000000000);
		float new_vitae = 1.0;		
		bool has_new_vitae = VitaeSystem::DetermineNewVitaeLevel(m_Qualities.GetVitaeValue(), InqIntQuality(DEATH_LEVEL_INT, 1), &vitae_pool, &new_vitae);

		UpdateVitaePool(vitae_pool);

		if (has_new_vitae)
		{
			if (new_vitae < 1.0f)
			{
				SendText("Your experience has reduced your Vitae penalty!", LTT_MAGIC);
			}

			Enchantment enchant;
			if (m_Qualities.InqVitae(&enchant))
			{
				enchant._smod.val = new_vitae;
				m_Qualities.UpdateEnchantment(&enchant);
			}

			UpdateVitaeEnchantment();

			CheckVitalRanges();
		}
	}
	else
	{
		// there should be no vitae...
		UpdateVitaeEnchantment();
	}
}

void CPlayerWeenie::OnDeath(DWORD killer_id)
{
	_recallTime = -1.0; // cancel any portal recalls

	m_bReviveAfterAnim = true;
	CMonsterWeenie::OnDeath(killer_id);
	
	m_Qualities.SetFloat(DEATH_TIMESTAMP_FLOAT, Timer::cur_time);
	NotifyFloatStatUpdated(DEATH_TIMESTAMP_FLOAT);

	m_Qualities.SetInt(DEATH_LEVEL_INT, m_Qualities.GetInt(LEVEL_INT, 1));
	NotifyIntStatUpdated(DEATH_LEVEL_INT, true);

	UpdateVitaePool(0);
	ReduceVitae(0.05f);
	UpdateVitaeEnchantment();

	if (killer_id != GetID())
	{
		if (CWeenieObject *pKiller = g_pWorld->FindObject(killer_id))
		{
			if (IsPK() && pKiller->_IsPlayer())
			{
				m_Qualities.SetFloat(PK_TIMESTAMP_FLOAT, Timer::cur_time + g_pConfig->PKRespiteTime());
				m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PKStatusEnum::NPK_PKStatus);
				NotifyIntStatUpdated(PLAYER_KILLER_STATUS_INT, false);

				SendText("Bael'Zharon has granted you respite after your moment of weakness. You are temporarily no longer a player killer.", LTT_MAGIC);
			}
		}
	}

	if (g_pConfig->HardcoreMode())
	{
		OnDeathAnimComplete();
	}
}

void CPlayerWeenie::NotifyAttackDone(int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1A7);
	msg.Write<int>(error); // no error
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyCommenceAttack()
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1B8);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::OnMotionDone(DWORD motion, BOOL success)
{
	CMonsterWeenie::OnMotionDone(motion, success);

	if (IsAttackMotion(motion) && success)
	{
		NotifyAttackDone();

		/*
		if (ShouldRepeatAttacks() && m_LastAttackTarget)
		{
			NotifyCommenceAttack();

			m_bChargingAttack = true;
			m_ChargingAttackHeight = m_LastAttackHeight;
			m_ChargingAttackTarget = m_LastAttackTarget;
			m_ChargingAttackPower = m_LastAttackPower;
			m_fChargeAttackStartTime = Timer::cur_time;
		}
		*/
	}
}

void CPlayerWeenie::NotifyAttackerEvent(const char *name, unsigned int dmgType, float healthPercent, unsigned int health, unsigned int crit, unsigned int attackConditions)
{
	// when the player deals damage
	BinaryWriter msg;
	msg.Write<DWORD>(0x1B1);
	msg.WriteString(name);
	msg.Write<DWORD>(dmgType);
	msg.Write<double>(healthPercent);
	msg.Write<DWORD>(health);
	msg.Write<DWORD>(crit);
	msg.Write<DWORD64>(attackConditions);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyDefenderEvent(const char *name, unsigned int dmgType, float healthPercent, unsigned int health, BODY_PART_ENUM hitPart, unsigned int crit, unsigned int attackConditions)
{
	// when the player receives damage
	BinaryWriter msg;
	msg.Write<DWORD>(0x1B2);
	msg.WriteString(name);
	msg.Write<DWORD>(dmgType);
	msg.Write<double>(healthPercent);
	msg.Write<DWORD>(health);
	msg.Write<int>(hitPart);
	msg.Write<DWORD>(crit);
	msg.Write<DWORD>(attackConditions);
	msg.Write<DWORD>(0); // probably DWORD align
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyKillerEvent(const char *text)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1AD);
	msg.WriteString(text);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyVictimEvent(const char *text)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1AC);
	msg.WriteString(text);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyUseDone(int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1C7);
	msg.Write<int>(error);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyWeenieError(int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x28A);
	msg.Write<int>(error);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyWeenieErrorWithString(int error, const char *text)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x28B);
	msg.Write<int>(error);
	msg.WriteString(text);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyInventoryFailedEvent(DWORD object_id, int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0xA0);
	msg.Write<DWORD>(object_id);
	msg.Write<int>(error);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

bool CPlayerWeenie::ImmuneToDamage(CWeenieObject *other)
{
	if (this != other)
	{
		if (other && other->AsPlayer())
		{
			if (IsPK() && other->IsPK())
			{
			}
			else if (IsPKLite() && other->IsPKLite())
			{
			}
			else
			{
				PKStatusEnum selfStatus = (PKStatusEnum) InqIntQuality(PLAYER_KILLER_STATUS_INT, PKStatusEnum::Undef_PKStatus);
				PKStatusEnum otherStatus = (PKStatusEnum) other->InqIntQuality(PLAYER_KILLER_STATUS_INT, PKStatusEnum::Undef_PKStatus);

				if (selfStatus == PKStatusEnum::Baelzharon_PKStatus || otherStatus == PKStatusEnum::Baelzharon_PKStatus)
				{
				}
				else
					return true;
			}
		}
	}

	return CMonsterWeenie::ImmuneToDamage(other);
}

bool CPlayerWeenie::IsDead()
{
	return CMonsterWeenie::IsDead();
}

DWORD CPlayerWeenie::ReceiveInventoryItem(CWeenieObject *source, CWeenieObject *item, DWORD desired_slot)
{
	item->ReleaseFromAnyWeenieParent(false, true);
	item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);

	item->SetWeenieContainer(GetID());
	item->ReleaseFromBlock();

	return Container_InsertInventoryItem(source->GetLandcell(), item, desired_slot);
}

/*
void CPlayerWeenie::HandleKilledEvent(CWeenieObject *victim, DAMAGE_TYPE damageType)
{
	switch (damageType)
	{
	case BLUDGEON_DAMAGE_TYPE:
		{
			switch (Random::GenInt(0, 7))
			{
			case 0:
				{
					NotifyKillerEvent(csprintf("You flatten %s's body with the force of your assault!", pTarget->GetName().c_str()));
					break;
				}
			case 1:
				{
					NotifyKillerEvent(csprintf("You beat %s to a lifeless pulp!", pTarget->GetName().c_str()));
					break;
				}
			case 2:
				{
					NotifyKillerEvent(csprintf("You smite %s mightily!", pTarget->GetName().c_str()));
					break;
				}
			case 3:
				{
					NotifyKillerEvent(csprintf("You knock %s into next Morningthaw!", pTarget->GetName().c_str()));
					break;
				}
			case 4:
				{
					NotifyKillerEvent(csprintf("%s is utterly destroyed by your attack!", pTarget->GetName().c_str()));
					break;
				}
			case 5:
				{
					NotifyKillerEvent(csprintf("%s catches your attack, with dire consequences!", pTarget->GetName().c_str()));
					break;
				}
			case 6:
				{
					NotifyKillerEvent(csprintf("The deadly force of your attack is so strong that %s's ancestors feel it!", pTarget->GetName().c_str()));
					break;
				}
			case 7:
				{
					NotifyKillerEvent(csprintf("The thunder of crushing %s is followed by the deafening silence of death!", pTarget->GetName().c_str()));
					break;
				}
			}
		}
	}
}
*/

/*
void CPlayerWeenie::NotifyVictimEvent(CWeenieObject *killer, DAMAGE_TYPE damageType)
{

}

void CPlayerWeenie::OnDealtDamage(CWeenieObject *attacker, DAMAGE_TYPE damageType, unsigned int damage)
{
	if (!pTarget->IsDead())
	{
		NotifyAttackerEvent(pTarget->GetName().c_str(), 4, damageDone / (double)(pTarget->GetMaxHealth()), damageDone, 0, 0);
	}
	else
	{
		switch (Random::GenInt(0, 7))
		{
		case 0:
			{
				NotifyKillerEvent(csprintf("You flatten %s's body with the force of your assault!", pTarget->GetName().c_str()));
				break;
			}
		case 1:
			{
				NotifyKillerEvent(csprintf("You beat %s to a lifeless pulp!", pTarget->GetName().c_str()));
				break;
			}
		case 2:
			{
				NotifyKillerEvent(csprintf("You smite %s mightily!", pTarget->GetName().c_str()));
				break;
			}
		case 3:
			{
				NotifyKillerEvent(csprintf("You knock %s into next Morningthaw!", pTarget->GetName().c_str()));
				break;
			}
		case 4:
			{
				NotifyKillerEvent(csprintf("%s is utterly destroyed by your attack!", pTarget->GetName().c_str()));
				break;
			}
		case 5:
			{
				NotifyKillerEvent(csprintf("%s catches your attack, with dire consequences!", pTarget->GetName().c_str()));
				break;
			}
		case 6:
			{
				NotifyKillerEvent(csprintf("The deadly force of your attack is so strong that %s's ancestors feel it!", pTarget->GetName().c_str()));
				break;
			}
		case 7:
			{
				NotifyKillerEvent(csprintf("The thunder of crushing %s is followed by the deafening silence of death!", pTarget->GetName().c_str()));
				break;
			}
		}
	}
}
*/

void CPlayerWeenie::PreSpawnCreate()
{
}

int CPlayerWeenie::UseEx(CWeenieObject *pTool, CWeenieObject *pTarget)
{
	CCraftOperation *op = g_pPortalDataEx->GetCraftOperation(pTool->m_Qualities.id, pTarget->m_Qualities.id);
	if (!op)
		return WERROR_NONE;

	CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(op->_successWcid, NULL, false);
	if (!weenie)
		return WERROR_NONE;

	if (op->_successAmount > 0)
	{
		weenie->SetStackSize(op->_successAmount);
	}

	weenie->SetID(g_pWorld->GenerateGUID(eDynamicGUID));

	if (g_pWorld->CreateEntity(weenie, false))
	{
		int error = CraftObject(this, weenie);

		if (error != WERROR_NONE)
		{
			g_pWorld->RemoveEntity(weenie);
		}
		else
		{
			SendText(op->_successMessage.c_str(), LTT_CRAFT);

			// target
			// source

			if (op->_comp1_1 == 1.0)
			{
				pTarget->DecrementStackOrStructureNum();
			}

			if (op->_comp2_1 == 1.0)
			{
				pTool->DecrementStackOrStructureNum();
			}
		}

		RecalculateEncumbrance();
	}

	return WERROR_NONE;
}

void CPlayerWeenie::SetLoginPlayerQualities()
{
	g_pAllegianceManager->SetWeenieAllegianceQualities(this);
	m_Qualities.SetFloat(LOGIN_TIMESTAMP_FLOAT, Timer::cur_time);

	// Position startPos = Position(0xDB75003B, Vector(186.000000f, 65.000000f, 36.088333f), Quaternion(1.000000, 0.000000, 0.000000, 0.000000));
	// Position startPos = Position(0xA9B4001F, Vector(87.750603f, 147.722321f, 66.005005f), Quaternion(0.011819f, 0.000000, 0.000000, -0.999930f));
	
	// Your location is: 0xC98C0028 [113.665604 190.259003 22.004999] -0.707107 0.000000 0.000000 -0.707107 Rithwic
	// Position startPos = Position(0xC98C0028, Vector(113.665604f, 190.259003f, 22.004999f), Quaternion(-0.707107f, 0.000000, 0.000000, -0.707107f));
	// SetInitialPosition(startPos);
	// m_Qualities.SetPosition(SANCTUARY_POSITION, startPos);

	extern bool g_bStartOverride;
	extern Position g_StartPosition;

	if (g_bStartOverride)
	{
		SetInitialPosition(g_StartPosition);
		m_Qualities.SetPosition(SANCTUARY_POSITION, g_StartPosition);
	}

	// should never be in a fellowship when logging in, but let's be sure
	m_Qualities.RemoveString(FELLOWSHIP_STRING);

	m_Qualities.SetInt(ARMOR_LEVEL_INT, 0);
	m_Qualities.SetInt(PHYSICS_STATE_INT, PhysicsState::HIDDEN_PS | PhysicsState::IGNORE_COLLISIONS_PS | PhysicsState::EDGE_SLIDE_PS | PhysicsState::GRAVITY_PS);
	m_Qualities.SetBool(ROT_PROOF_BOOL, FALSE);
	m_Qualities.RemoveInt(RADARBLIP_COLOR_INT);

	if (atoi(g_pConfig->GetValue("player_killer_only", "0")) != 0)
	{
		double pkTimestamp;
		if (!m_Qualities.InqFloat(PK_TIMESTAMP_FLOAT, pkTimestamp, TRUE))
		{
			m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PK_PKStatus);
		}
	}

	if (IsAdvocate())
	{
		m_Qualities.SetBool(IS_ADVOCATE_BOOL, TRUE);
		m_Qualities.SetBool(ADVOCATE_STATE_BOOL, TRUE);
	}
	else
	{
		m_Qualities.RemoveBool(IS_ADVOCATE_BOOL);
		m_Qualities.RemoveBool(ADVOCATE_STATE_BOOL);
	}

	if (IsSentinel())
	{
		m_Qualities.SetBool(IS_SENTINEL_BOOL, TRUE);
	}
	else
	{
		m_Qualities.RemoveBool(IS_SENTINEL_BOOL);
	}

	if (IsAdmin())
	{
		m_Qualities.SetBool(IS_ADMIN_BOOL, TRUE);
		m_Qualities.SetBool(IS_ARCH_BOOL, TRUE);
	}
	else
	{
		m_Qualities.RemoveBool(IS_ADMIN_BOOL);
		m_Qualities.RemoveBool(IS_ARCH_BOOL);
	}

	m_Qualities.SetBool(SPELL_COMPONENTS_REQUIRED_BOOL, FALSE);

	/*
	if (m_Qualities._spell_book)
	{
		delete m_Qualities._spell_book;
		m_Qualities._spell_book = NULL;
	}
	*/

	m_Qualities.AddSpell(WhirlingBlade1_SpellID);
	m_Qualities.AddSpell(ForceBolt1_SpellID);
	m_Qualities.AddSpell(ShockWave1_SpellID);
	m_Qualities.AddSpell(FlameBolt1_SpellID);
	m_Qualities.AddSpell(FrostBolt1_SpellID);
	m_Qualities.AddSpell(AcidStream1_SpellID);
	m_Qualities.AddSpell(LightningBolt1_SpellID);

	m_Qualities.AddSpell(BladeArc1_SpellID);
	m_Qualities.AddSpell(ForceArc1_SpellID);
	m_Qualities.AddSpell(ShockArc1_SpellID);
	m_Qualities.AddSpell(FlameArc1_SpellID);
	m_Qualities.AddSpell(FrostArc1_SpellID);
	m_Qualities.AddSpell(AcidArc1_SpellID);
	m_Qualities.AddSpell(LightningArc1_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak1_SpellID);
	m_Qualities.AddSpell(ForceStreak1_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak1_SpellID);
	m_Qualities.AddSpell(FlameStreak1_SpellID);
	m_Qualities.AddSpell(FrostStreak1_SpellID);
	m_Qualities.AddSpell(AcidStreak1_SpellID);
	m_Qualities.AddSpell(LightningStreak1_SpellID);

	m_Qualities.AddSpell(WhirlingBlade2_SpellID);
	m_Qualities.AddSpell(ForceBolt2_SpellID);
	m_Qualities.AddSpell(ShockWave2_SpellID);
	m_Qualities.AddSpell(FlameBolt2_SpellID);
	m_Qualities.AddSpell(FrostBolt2_SpellID);
	m_Qualities.AddSpell(AcidStream2_SpellID);
	m_Qualities.AddSpell(LightningBolt2_SpellID);

	m_Qualities.AddSpell(BladeArc2_SpellID);
	m_Qualities.AddSpell(ForceArc2_SpellID);
	m_Qualities.AddSpell(ShockArc2_SpellID);
	m_Qualities.AddSpell(FlameArc2_SpellID);
	m_Qualities.AddSpell(FrostArc2_SpellID);
	m_Qualities.AddSpell(AcidArc2_SpellID);
	m_Qualities.AddSpell(LightningArc2_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak2_SpellID);
	m_Qualities.AddSpell(ForceStreak2_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak2_SpellID);
	m_Qualities.AddSpell(FlameStreak2_SpellID);
	m_Qualities.AddSpell(FrostStreak2_SpellID);
	m_Qualities.AddSpell(AcidStreak2_SpellID);
	m_Qualities.AddSpell(LightningStreak2_SpellID);

	m_Qualities.AddSpell(WhirlingBlade3_SpellID);
	m_Qualities.AddSpell(ForceBolt3_SpellID);
	m_Qualities.AddSpell(ShockWave3_SpellID);
	m_Qualities.AddSpell(FlameBolt3_SpellID);
	m_Qualities.AddSpell(FrostBolt3_SpellID);
	m_Qualities.AddSpell(AcidStream3_SpellID);
	m_Qualities.AddSpell(LightningBolt3_SpellID);

	m_Qualities.AddSpell(BladeArc3_SpellID);
	m_Qualities.AddSpell(ForceArc3_SpellID);
	m_Qualities.AddSpell(ShockArc3_SpellID);
	m_Qualities.AddSpell(FlameArc3_SpellID);
	m_Qualities.AddSpell(FrostArc3_SpellID);
	m_Qualities.AddSpell(AcidArc3_SpellID);
	m_Qualities.AddSpell(LightningArc3_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak3_SpellID);
	m_Qualities.AddSpell(ForceStreak3_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak3_SpellID);
	m_Qualities.AddSpell(FlameStreak3_SpellID);
	m_Qualities.AddSpell(FrostStreak3_SpellID);
	m_Qualities.AddSpell(AcidStreak3_SpellID);
	m_Qualities.AddSpell(LightningStreak3_SpellID);

	m_Qualities.AddSpell(WhirlingBlade4_SpellID);
	m_Qualities.AddSpell(ForceBolt4_SpellID);
	m_Qualities.AddSpell(ShockWave4_SpellID);
	m_Qualities.AddSpell(FlameBolt4_SpellID);
	m_Qualities.AddSpell(FrostBolt4_SpellID);
	m_Qualities.AddSpell(AcidStream4_SpellID);
	m_Qualities.AddSpell(LightningBolt4_SpellID);

	m_Qualities.AddSpell(BladeArc4_SpellID);
	m_Qualities.AddSpell(ForceArc4_SpellID);
	m_Qualities.AddSpell(ShockArc4_SpellID);
	m_Qualities.AddSpell(FlameArc4_SpellID);
	m_Qualities.AddSpell(FrostArc4_SpellID);
	m_Qualities.AddSpell(AcidArc4_SpellID);
	m_Qualities.AddSpell(LightningArc4_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak4_SpellID);
	m_Qualities.AddSpell(ForceStreak4_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak4_SpellID);
	m_Qualities.AddSpell(FlameStreak4_SpellID);
	m_Qualities.AddSpell(FrostStreak4_SpellID);
	m_Qualities.AddSpell(AcidStreak4_SpellID);
	m_Qualities.AddSpell(LightningStreak4_SpellID);

	m_Qualities.AddSpell(WhirlingBlade5_SpellID);
	m_Qualities.AddSpell(ForceBolt5_SpellID);
	m_Qualities.AddSpell(ShockWave5_SpellID);
	m_Qualities.AddSpell(FlameBolt5_SpellID);
	m_Qualities.AddSpell(FrostBolt5_SpellID);
	m_Qualities.AddSpell(AcidStream5_SpellID);
	m_Qualities.AddSpell(LightningBolt5_SpellID);

	m_Qualities.AddSpell(BladeArc5_SpellID);
	m_Qualities.AddSpell(ForceArc5_SpellID);
	m_Qualities.AddSpell(ShockArc5_SpellID);
	m_Qualities.AddSpell(FlameArc5_SpellID);
	m_Qualities.AddSpell(FrostArc5_SpellID);
	m_Qualities.AddSpell(AcidArc5_SpellID);
	m_Qualities.AddSpell(LightningArc5_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak5_SpellID);
	m_Qualities.AddSpell(ForceStreak5_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak5_SpellID);
	m_Qualities.AddSpell(FlameStreak5_SpellID);
	m_Qualities.AddSpell(FrostStreak5_SpellID);
	m_Qualities.AddSpell(AcidStreak5_SpellID);
	m_Qualities.AddSpell(LightningStreak5_SpellID);

	m_Qualities.AddSpell(WhirlingBlade6_SpellID);
	m_Qualities.AddSpell(ForceBolt6_SpellID);
	m_Qualities.AddSpell(ShockWave6_SpellID);
	m_Qualities.AddSpell(FlameBolt6_SpellID);
	m_Qualities.AddSpell(FrostBolt6_SpellID);
	m_Qualities.AddSpell(AcidStream6_SpellID);
	m_Qualities.AddSpell(LightningBolt6_SpellID);

	m_Qualities.AddSpell(BladeArc6_SpellID);
	m_Qualities.AddSpell(ForceArc6_SpellID);
	m_Qualities.AddSpell(ShockArc6_SpellID);
	m_Qualities.AddSpell(FlameArc6_SpellID);
	m_Qualities.AddSpell(FrostArc6_SpellID);
	m_Qualities.AddSpell(AcidArc6_SpellID);
	m_Qualities.AddSpell(LightningArc6_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak6_SpellID);
	m_Qualities.AddSpell(ForceStreak6_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak6_SpellID);
	m_Qualities.AddSpell(FlameStreak6_SpellID);
	m_Qualities.AddSpell(FrostStreak6_SpellID);
	m_Qualities.AddSpell(AcidStreak6_SpellID);
	m_Qualities.AddSpell(LightningStreak6_SpellID);

	m_Qualities.AddSpell(Whirlingblade7_SpellID);
	m_Qualities.AddSpell(ForceBolt7_SpellID);
	m_Qualities.AddSpell(Shockwave7_SpellID);
	m_Qualities.AddSpell(FlameBolt7_SpellID);
	m_Qualities.AddSpell(FrostBolt7_SpellID);
	m_Qualities.AddSpell(AcidStream7_SpellID);
	m_Qualities.AddSpell(Lightningbolt7_SpellID);

	m_Qualities.AddSpell(BladeArc7_SpellID);
	m_Qualities.AddSpell(ForceArc7_SpellID);
	m_Qualities.AddSpell(ShockArc7_SpellID);
	m_Qualities.AddSpell(FlameArc7_SpellID);
	m_Qualities.AddSpell(FrostArc7_SpellID);
	m_Qualities.AddSpell(AcidArc7_SpellID);
	m_Qualities.AddSpell(LightningArc7_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak7_SpellID);
	m_Qualities.AddSpell(ForceStreak7_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak7_SpellID);
	m_Qualities.AddSpell(FlameStreak7_SpellID);
	m_Qualities.AddSpell(FrostStreak7_SpellID);
	m_Qualities.AddSpell(AcidStreak7_SpellID);
	m_Qualities.AddSpell(LightningStreak7_SpellID);

	/*
	m_Qualities.AddSpell(WhirlingBlade8_SpellID);
	m_Qualities.AddSpell(ForceBolt8_SpellID);
	m_Qualities.AddSpell(ShockWave8_SpellID);
	m_Qualities.AddSpell(FlameBolt8_SpellID);
	m_Qualities.AddSpell(FrostBolt8_SpellID);
	m_Qualities.AddSpell(AcidStream8_SpellID);
	m_Qualities.AddSpell(LightningBolt8_SpellID);

	m_Qualities.AddSpell(BladeArc8_SpellID);
	m_Qualities.AddSpell(ForceArc8_SpellID);
	m_Qualities.AddSpell(ShockArc8_SpellID);
	m_Qualities.AddSpell(FlameArc8_SpellID);
	m_Qualities.AddSpell(FrostArc8_SpellID);
	m_Qualities.AddSpell(AcidArc8_SpellID);
	m_Qualities.AddSpell(LightningArc8_SpellID);

	m_Qualities.AddSpell(WhirlingBladeStreak8_SpellID);
	m_Qualities.AddSpell(ForceStreak8_SpellID);
	m_Qualities.AddSpell(ShockwaveStreak8_SpellID);
	m_Qualities.AddSpell(FlameStreak8_SpellID);
	m_Qualities.AddSpell(FrostStreak8_SpellID);
	m_Qualities.AddSpell(AcidStreak8_SpellID);
	m_Qualities.AddSpell(LightningStreak8_SpellID);
	*/

	m_Qualities.AddSpell(HealSelf1_SpellID);
	m_Qualities.AddSpell(HealSelf2_SpellID);
	m_Qualities.AddSpell(HealSelf3_SpellID);
	m_Qualities.AddSpell(HealSelf4_SpellID);
	m_Qualities.AddSpell(HealSelf5_SpellID);
	m_Qualities.AddSpell(HealSelf6_SpellID);
	m_Qualities.AddSpell(healself7_SpellID);
	// m_Qualities.AddSpell(HealSelf8_SpellID);

	m_Qualities.AddSpell(HealOther1_SpellID);
	m_Qualities.AddSpell(HealOther2_SpellID);
	m_Qualities.AddSpell(HealOther3_SpellID);
	m_Qualities.AddSpell(HealOther4_SpellID);
	m_Qualities.AddSpell(HealOther5_SpellID);
	m_Qualities.AddSpell(HealOther6_SpellID);
	m_Qualities.AddSpell(healother7_SpellID);
	// m_Qualities.AddSpell(HealOther8_SpellID);

	m_Qualities.AddSpell(RevitalizeSelf1_SpellID);
	m_Qualities.AddSpell(RevitalizeSelf2_SpellID);
	m_Qualities.AddSpell(RevitalizeSelf3_SpellID);
	m_Qualities.AddSpell(RevitalizeSelf4_SpellID);
	m_Qualities.AddSpell(RevitalizeSelf5_SpellID);
	m_Qualities.AddSpell(RevitalizeSelf6_SpellID);
	m_Qualities.AddSpell(RevitalizeSelf7_SpellID);
	// m_Qualities.AddSpell(RevitalizeSelf8_SpellID);

	m_Qualities.AddSpell(RevitalizeOther1_SpellID);
	m_Qualities.AddSpell(RevitalizeOther2_SpellID);
	m_Qualities.AddSpell(RevitalizeOther3_SpellID);
	m_Qualities.AddSpell(RevitalizeOther4_SpellID);
	m_Qualities.AddSpell(RevitalizeOther5_SpellID);
	m_Qualities.AddSpell(RevitalizeOther6_SpellID);
	m_Qualities.AddSpell(Revitalizeother7_SpellID);
	// m_Qualities.AddSpell(RevitalizeOther8_SpellID);

	m_Qualities.AddSpell(DrainHealth1_SpellID);
	m_Qualities.AddSpell(DrainHealth2_SpellID);
	m_Qualities.AddSpell(DrainHealth3_SpellID);
	m_Qualities.AddSpell(DrainHealth4_SpellID);
	m_Qualities.AddSpell(DrainHealth5_SpellID);
	m_Qualities.AddSpell(DrainHealth6_SpellID);
	m_Qualities.AddSpell(DrainHealth7_SpellID);
	// m_Qualities.AddSpell(DrainHealth8_SpellID);

	m_Qualities.AddSpell(DrainStamina1_SpellID);
	m_Qualities.AddSpell(DrainStamina2_SpellID);
	m_Qualities.AddSpell(DrainStamina3_SpellID);
	m_Qualities.AddSpell(DrainStamina4_SpellID);
	m_Qualities.AddSpell(DrainStamina5_SpellID);
	m_Qualities.AddSpell(DrainStamina6_SpellID);
	m_Qualities.AddSpell(DrainStamina7_SpellID);
	// m_Qualities.AddSpell(DrainStamina8_SpellID);

	m_Qualities.AddSpell(DrainMana1_SpellID); // %
	m_Qualities.AddSpell(DrainMana2_SpellID);
	m_Qualities.AddSpell(DrainMana3_SpellID);
	m_Qualities.AddSpell(DrainMana4_SpellID);
	m_Qualities.AddSpell(DrainMana5_SpellID);
	m_Qualities.AddSpell(DrainMana6_SpellID);
	m_Qualities.AddSpell(DrainMana7_SpellID);
	// m_Qualities.AddSpell(DrainMana8_SpellID);

	m_Qualities.AddSpell(HarmOther1_SpellID);
	m_Qualities.AddSpell(HarmOther2_SpellID);
	m_Qualities.AddSpell(HarmOther3_SpellID);
	m_Qualities.AddSpell(HarmOther4_SpellID);
	m_Qualities.AddSpell(HarmOther5_SpellID);
	m_Qualities.AddSpell(HarmOther6_SpellID);
	m_Qualities.AddSpell(HarmOther7_SpellID);
	// m_Qualities.AddSpell(HarmOther8_SpellID);

	m_Qualities.AddSpell(EnfeebleOther1_SpellID);
	m_Qualities.AddSpell(EnfeebleOther2_SpellID);
	m_Qualities.AddSpell(EnfeebleOther3_SpellID);
	m_Qualities.AddSpell(EnfeebleOther4_SpellID);
	m_Qualities.AddSpell(EnfeebleOther5_SpellID);
	m_Qualities.AddSpell(EnfeebleOther6_SpellID);
	m_Qualities.AddSpell(EnfeebleOther7_SpellID);
	// m_Qualities.AddSpell(EnfeebleOther8_SpellID);

	m_Qualities.AddSpell(ManaDrainOther1_SpellID); // #
	m_Qualities.AddSpell(ManaDrainOther2_SpellID);
	m_Qualities.AddSpell(ManaDrainOther3_SpellID);
	m_Qualities.AddSpell(ManaDrainOther4_SpellID);
	m_Qualities.AddSpell(ManaDrainOther5_SpellID);
	m_Qualities.AddSpell(ManaDrainOther6_SpellID);
	m_Qualities.AddSpell(ManaDrainOther7_SpellID);
	// m_Qualities.AddSpell(ManaDrainOther8_SpellID);

	m_Qualities.AddSpell(InfuseHealth1_SpellID);
	m_Qualities.AddSpell(InfuseHealth2_SpellID);
	m_Qualities.AddSpell(InfuseHealth3_SpellID);
	m_Qualities.AddSpell(InfuseHealth4_SpellID);
	m_Qualities.AddSpell(InfuseHealth5_SpellID);
	m_Qualities.AddSpell(InfuseHealth6_SpellID);
	m_Qualities.AddSpell(InfuseHealth7_SpellID);
	// m_Qualities.AddSpell(InfuseHealth8_SpellID);

	m_Qualities.AddSpell(InfuseStamina1_SpellID);
	m_Qualities.AddSpell(InfuseStamina2_SpellID);
	m_Qualities.AddSpell(InfuseStamina3_SpellID);
	m_Qualities.AddSpell(InfuseStamina4_SpellID);
	m_Qualities.AddSpell(InfuseStamina5_SpellID);
	m_Qualities.AddSpell(InfuseStamina6_SpellID);
	m_Qualities.AddSpell(InfuseStamina7_SpellID);
	// m_Qualities.AddSpell(InfuseStamina8_SpellID);

	m_Qualities.AddSpell(InfuseMana1_SpellID);
	m_Qualities.AddSpell(InfuseMana2_SpellID);
	m_Qualities.AddSpell(InfuseMana3_SpellID);
	m_Qualities.AddSpell(InfuseMana4_SpellID);
	m_Qualities.AddSpell(InfuseMana5_SpellID);
	m_Qualities.AddSpell(InfuseMana6_SpellID);
	m_Qualities.AddSpell(InfuseMana7_SpellID);
	// m_Qualities.AddSpell(InfuseMana8_SpellID);

	//m_Qualities.AddSpell(AcidRing_SpellID);
	//m_Qualities.AddSpell(BladeRing_SpellID);
	m_Qualities.AddSpell(FlameRing_SpellID);
	//m_Qualities.AddSpell(ForceRing_SpellID);
	//m_Qualities.AddSpell(FrostRing_SpellID);
	m_Qualities.AddSpell(LightningRing_SpellID);
	//m_Qualities.AddSpell(ShockwaveRing_SpellID);

	m_Qualities.AddSpell(StrengthSelf1_SpellID);
	m_Qualities.AddSpell(StrengthSelf2_SpellID);
	m_Qualities.AddSpell(StrengthSelf3_SpellID);
	m_Qualities.AddSpell(StrengthSelf4_SpellID);
	m_Qualities.AddSpell(StrengthSelf5_SpellID);
	m_Qualities.AddSpell(StrengthSelf6_SpellID);
	m_Qualities.AddSpell(StrengthSelf7_SpellID);
	m_Qualities.AddSpell(StrengthOther1_SpellID);
	m_Qualities.AddSpell(StrengthOther2_SpellID);
	m_Qualities.AddSpell(StrengthOther3_SpellID);
	m_Qualities.AddSpell(StrengthOther4_SpellID);
	m_Qualities.AddSpell(StrengthOther5_SpellID);
	m_Qualities.AddSpell(StrengthOther6_SpellID);
	m_Qualities.AddSpell(StrengthOther7_SpellID);
	m_Qualities.AddSpell(EnduranceSelf1_SpellID);
	m_Qualities.AddSpell(EnduranceSelf2_SpellID);
	m_Qualities.AddSpell(EnduranceSelf3_SpellID);
	m_Qualities.AddSpell(EnduranceSelf4_SpellID);
	m_Qualities.AddSpell(EnduranceSelf5_SpellID);
	m_Qualities.AddSpell(EnduranceSelf6_SpellID);
	m_Qualities.AddSpell(EnduranceSelf7_SpellID);
	m_Qualities.AddSpell(EnduranceOther1_SpellID);
	m_Qualities.AddSpell(EnduranceOther2_SpellID);
	m_Qualities.AddSpell(EnduranceOther3_SpellID);
	m_Qualities.AddSpell(EnduranceOther4_SpellID);
	m_Qualities.AddSpell(EnduranceOther5_SpellID);
	m_Qualities.AddSpell(EnduranceOther6_SpellID);
	m_Qualities.AddSpell(EnduranceOther7_SpellID);
	m_Qualities.AddSpell(CoordinationSelf1_SpellID);
	m_Qualities.AddSpell(CoordinationSelf2_SpellID);
	m_Qualities.AddSpell(CoordinationSelf3_SpellID);
	m_Qualities.AddSpell(CoordinationSelf4_SpellID);
	m_Qualities.AddSpell(CoordinationSelf5_SpellID);
	m_Qualities.AddSpell(CoordinationSelf6_SpellID);
	m_Qualities.AddSpell(CoordinationSelf7_SpellID);
	m_Qualities.AddSpell(CoordinationOther1_SpellID);
	m_Qualities.AddSpell(CoordinationOther2_SpellID);
	m_Qualities.AddSpell(CoordinationOther3_SpellID);
	m_Qualities.AddSpell(CoordinationOther4_SpellID);
	m_Qualities.AddSpell(CoordinationOther5_SpellID);
	m_Qualities.AddSpell(CoordinationOther6_SpellID);
	m_Qualities.AddSpell(CoordinationOther7_SpellID);
	m_Qualities.AddSpell(QuicknessSelf1_SpellID);
	m_Qualities.AddSpell(QuicknessSelf2_SpellID);
	m_Qualities.AddSpell(QuicknessSelf3_SpellID);
	m_Qualities.AddSpell(QuicknessSelf4_SpellID);
	m_Qualities.AddSpell(QuicknessSelf5_SpellID);
	m_Qualities.AddSpell(QuicknessSelf6_SpellID);
	m_Qualities.AddSpell(QuicknessSelf7_SpellID);
	m_Qualities.AddSpell(QuicknessOther1_SpellID);
	m_Qualities.AddSpell(QuicknessOther2_SpellID);
	m_Qualities.AddSpell(QuicknessOther3_SpellID);
	m_Qualities.AddSpell(QuicknessOther4_SpellID);
	m_Qualities.AddSpell(QuicknessOther5_SpellID);
	m_Qualities.AddSpell(QuicknessOther6_SpellID);
	m_Qualities.AddSpell(QuicknessOther7_SpellID);
	m_Qualities.AddSpell(FocusSelf1_SpellID);
	m_Qualities.AddSpell(FocusSelf2_SpellID);
	m_Qualities.AddSpell(FocusSelf3_SpellID);
	m_Qualities.AddSpell(FocusSelf4_SpellID);
	m_Qualities.AddSpell(FocusSelf5_SpellID);
	m_Qualities.AddSpell(FocusSelf6_SpellID);
	m_Qualities.AddSpell(FocusSelf7_SpellID);
	m_Qualities.AddSpell(FocusOther1_SpellID);
	m_Qualities.AddSpell(FocusOther2_SpellID);
	m_Qualities.AddSpell(FocusOther3_SpellID);
	m_Qualities.AddSpell(FocusOther4_SpellID);
	m_Qualities.AddSpell(FocusOther5_SpellID);
	m_Qualities.AddSpell(FocusOther6_SpellID);
	m_Qualities.AddSpell(FocusOther7_SpellID);
	m_Qualities.AddSpell(WillpowerSelf1_SpellID);
	m_Qualities.AddSpell(WillpowerSelf2_SpellID);
	m_Qualities.AddSpell(WillpowerSelf3_SpellID);
	m_Qualities.AddSpell(WillpowerSelf4_SpellID);
	m_Qualities.AddSpell(WillpowerSelf5_SpellID);
	m_Qualities.AddSpell(WillpowerSelf6_SpellID);
	m_Qualities.AddSpell(WillpowerSelf7_SpellID);
	m_Qualities.AddSpell(WillpowerOther1_SpellID);
	m_Qualities.AddSpell(WillpowerOther2_SpellID);
	m_Qualities.AddSpell(WillpowerOther3_SpellID);
	m_Qualities.AddSpell(WillpowerOther4_SpellID);
	m_Qualities.AddSpell(WillpowerOther5_SpellID);
	m_Qualities.AddSpell(WillpowerOther6_SpellID);
	m_Qualities.AddSpell(WillPowerOther7_SpellID);

	m_Qualities.AddSpell(SprintSelf1_SpellID);
	m_Qualities.AddSpell(SprintSelf2_SpellID);
	m_Qualities.AddSpell(SprintSelf3_SpellID);
	m_Qualities.AddSpell(SprintSelf4_SpellID);
	m_Qualities.AddSpell(SprintSelf5_SpellID);
	m_Qualities.AddSpell(SprintSelf6_SpellID);
	m_Qualities.AddSpell(SprintSelf7_SpellID);;
	m_Qualities.AddSpell(SprintOther1_SpellID);
	m_Qualities.AddSpell(SprintOther2_SpellID);
	m_Qualities.AddSpell(SprintOther3_SpellID);
	m_Qualities.AddSpell(SprintOther4_SpellID);
	m_Qualities.AddSpell(SprintOther5_SpellID);
	m_Qualities.AddSpell(SprintOther6_SpellID);
	m_Qualities.AddSpell(SprintOther7_SpellID);;

	m_Qualities.AddSpell(JumpingMasterySelf1_SpellID);
	m_Qualities.AddSpell(JumpingMasterySelf2_SpellID);
	m_Qualities.AddSpell(JumpingMasterySelf3_SpellID);
	m_Qualities.AddSpell(JumpingMasterySelf4_SpellID);
	m_Qualities.AddSpell(JumpingMasterySelf5_SpellID);
	m_Qualities.AddSpell(JumpingMasterySelf6_SpellID);
	m_Qualities.AddSpell(JumpingMasterySelf7_SpellID);;
	m_Qualities.AddSpell(JumpingMasteryOther1_SpellID);
	m_Qualities.AddSpell(JumpingMasteryOther2_SpellID);
	m_Qualities.AddSpell(JumpingMasteryOther3_SpellID);
	m_Qualities.AddSpell(JumpingMasteryOther4_SpellID);
	m_Qualities.AddSpell(JumpingMasteryOther5_SpellID);
	m_Qualities.AddSpell(JumpingMasteryOther6_SpellID);
	m_Qualities.AddSpell(JumpingMasteryOther7_SpellID);

	m_Qualities.AddSpell(ManaMasterySelf1_SpellID);
	m_Qualities.AddSpell(ManaMasterySelf2_SpellID);
	m_Qualities.AddSpell(ManaMasterySelf3_SpellID);
	m_Qualities.AddSpell(ManaMasterySelf4_SpellID);
	m_Qualities.AddSpell(ManaMasterySelf5_SpellID);
	m_Qualities.AddSpell(ManaMasterySelf6_SpellID);
	m_Qualities.AddSpell(ManaMasterySelf7_SpellID);;
	m_Qualities.AddSpell(ManaMasteryOther1_SpellID);
	m_Qualities.AddSpell(ManaMasteryOther2_SpellID);
	m_Qualities.AddSpell(ManaMasteryOther3_SpellID);
	m_Qualities.AddSpell(ManaMasteryOther4_SpellID);
	m_Qualities.AddSpell(ManaMasteryOther5_SpellID);
	m_Qualities.AddSpell(ManaMasteryOther6_SpellID);
	m_Qualities.AddSpell(ManaMasteryOther7_SpellID);

	m_Qualities.AddSpell(InvulnerabilitySelf1_SpellID);
	m_Qualities.AddSpell(InvulnerabilitySelf2_SpellID);
	m_Qualities.AddSpell(InvulnerabilitySelf3_SpellID);
	m_Qualities.AddSpell(InvulnerabilitySelf4_SpellID);
	m_Qualities.AddSpell(InvulnerabilitySelf5_SpellID);
	m_Qualities.AddSpell(InvulnerabilitySelf6_SpellID);
	m_Qualities.AddSpell(InvulnerabilitySelf7_SpellID);;
	m_Qualities.AddSpell(InvulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(InvulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(InvulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(InvulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(InvulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(InvulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(InvulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther1_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther2_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther3_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther4_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther5_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther6_SpellID);
	m_Qualities.AddSpell(DefenselessnessOther7_SpellID);

	m_Qualities.AddSpell(ImpregnabilitySelf1_SpellID);
	m_Qualities.AddSpell(ImpregnabilitySelf2_SpellID);
	m_Qualities.AddSpell(ImpregnabilitySelf3_SpellID);
	m_Qualities.AddSpell(ImpregnabilitySelf4_SpellID);
	m_Qualities.AddSpell(ImpregnabilitySelf5_SpellID);
	m_Qualities.AddSpell(ImpregnabilitySelf6_SpellID);
	m_Qualities.AddSpell(ImpregnabilitySelf7_SpellID);;
	m_Qualities.AddSpell(ImpregnabilityOther1_SpellID);
	m_Qualities.AddSpell(ImpregnabilityOther2_SpellID);
	m_Qualities.AddSpell(ImpregnabilityOther3_SpellID);
	m_Qualities.AddSpell(ImpregnabilityOther4_SpellID);
	m_Qualities.AddSpell(ImpregnabilityOther5_SpellID);
	m_Qualities.AddSpell(ImpregnabilityOther6_SpellID);
	m_Qualities.AddSpell(ImpregnabilityOther7_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(VulnerabilityOther7_SpellID);

	m_Qualities.AddSpell(MagicResistanceSelf1_SpellID);
	m_Qualities.AddSpell(MagicResistanceSelf2_SpellID);
	m_Qualities.AddSpell(MagicResistanceSelf3_SpellID);
	m_Qualities.AddSpell(MagicResistanceSelf4_SpellID);
	m_Qualities.AddSpell(MagicResistanceSelf5_SpellID);
	m_Qualities.AddSpell(MagicResistanceSelf6_SpellID);
	m_Qualities.AddSpell(MagicResistanceSelf7_SpellID);;
	m_Qualities.AddSpell(MagicResistanceOther1_SpellID);
	m_Qualities.AddSpell(MagicResistanceOther2_SpellID);
	m_Qualities.AddSpell(MagicResistanceOther3_SpellID);
	m_Qualities.AddSpell(MagicResistanceOther4_SpellID);
	m_Qualities.AddSpell(MagicResistanceOther5_SpellID);
	m_Qualities.AddSpell(MagicResistanceOther6_SpellID);
	m_Qualities.AddSpell(MagicResistanceOther7_SpellID);
	m_Qualities.AddSpell(MagicYieldOther1_SpellID);
	m_Qualities.AddSpell(MagicYieldOther2_SpellID);
	m_Qualities.AddSpell(MagicYieldOther3_SpellID);
	m_Qualities.AddSpell(MagicYieldOther4_SpellID);
	m_Qualities.AddSpell(MagicYieldOther5_SpellID);
	m_Qualities.AddSpell(MagicYieldOther6_SpellID);
	m_Qualities.AddSpell(MagicYieldOther7_SpellID);

	m_Qualities.AddSpell(BladeVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther1_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf1_SpellID);
	m_Qualities.AddSpell(BladeVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther2_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf2_SpellID);
	m_Qualities.AddSpell(BladeVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther3_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf3_SpellID);
	m_Qualities.AddSpell(BladeVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther4_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf4_SpellID);
	m_Qualities.AddSpell(BladeVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther5_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf5_SpellID);
	m_Qualities.AddSpell(BladeVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther6_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf6_SpellID);
	m_Qualities.AddSpell(BladeVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(BladeProtectionOther7_SpellID);
	m_Qualities.AddSpell(BladeProtectionSelf7_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther1_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf1_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther2_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf2_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther3_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf3_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther4_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf4_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther5_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf5_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther6_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf6_SpellID);
	m_Qualities.AddSpell(PiercingVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(PiercingProtectionOther7_SpellID);
	m_Qualities.AddSpell(PiercingProtectionSelf7_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther1_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf1_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther2_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf2_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther3_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf3_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther4_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf4_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther5_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf5_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther6_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf6_SpellID);
	m_Qualities.AddSpell(BludgeonVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionOther7_SpellID);
	m_Qualities.AddSpell(BludgeonProtectionSelf7_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(FireProtectionOther1_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf1_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(FireProtectionOther2_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf2_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(FireProtectionOther3_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf3_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(FireProtectionOther4_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf4_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(FireProtectionOther5_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf5_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(FireProtectionOther6_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf6_SpellID);
	m_Qualities.AddSpell(FireVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(FireProtectionOther7_SpellID);
	m_Qualities.AddSpell(FireProtectionSelf7_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther1_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf1_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther2_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf2_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther3_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf3_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther4_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf4_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther5_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf5_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther6_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf6_SpellID);
	m_Qualities.AddSpell(ColdVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(ColdProtectionOther7_SpellID);
	m_Qualities.AddSpell(ColdProtectionSelf7_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther1_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf1_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther2_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf2_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther3_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf3_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther4_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf4_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther5_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf5_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther6_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf6_SpellID);
	m_Qualities.AddSpell(AcidVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(AcidProtectionOther7_SpellID);
	m_Qualities.AddSpell(AcidProtectionSelf7_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther1_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther1_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf1_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther2_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther2_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf2_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther3_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther3_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf3_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther4_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther4_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf4_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther5_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther5_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf5_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther6_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther6_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf6_SpellID);
	m_Qualities.AddSpell(LightningVulnerabilityOther7_SpellID);
	m_Qualities.AddSpell(LightningProtectionOther7_SpellID);
	m_Qualities.AddSpell(LightningProtectionSelf7_SpellID);

	m_Qualities.AddSpell(BladeBlast3_SpellID);
	m_Qualities.AddSpell(BladeBlast4_SpellID);
	m_Qualities.AddSpell(BladeBlast5_SpellID);
	m_Qualities.AddSpell(BladeBlast6_SpellID);
	m_Qualities.AddSpell(BladeBlast7_SpellID);

	m_Qualities.AddSpell(ShockBlast3_SpellID);
	m_Qualities.AddSpell(ShockBlast4_SpellID);
	m_Qualities.AddSpell(ShockBlast5_SpellID);
	m_Qualities.AddSpell(ShockBlast6_SpellID);
	m_Qualities.AddSpell(Shockblast7_SpellID);

	m_Qualities.AddSpell(ForceBlast3_SpellID);
	m_Qualities.AddSpell(ForceBlast4_SpellID);
	m_Qualities.AddSpell(ForceBlast5_SpellID);
	m_Qualities.AddSpell(ForceBlast6_SpellID);
	m_Qualities.AddSpell(ForceBlast7_SpellID);

	m_Qualities.AddSpell(FlameBlast3_SpellID);
	m_Qualities.AddSpell(FlameBlast4_SpellID);
	m_Qualities.AddSpell(FlameBlast5_SpellID);
	m_Qualities.AddSpell(FlameBlast6_SpellID);
	m_Qualities.AddSpell(FlameBlast7_SpellID);

	m_Qualities.AddSpell(FrostBlast3_SpellID);
	m_Qualities.AddSpell(FrostBlast4_SpellID);
	m_Qualities.AddSpell(FrostBlast5_SpellID);
	m_Qualities.AddSpell(FrostBlast6_SpellID);
	m_Qualities.AddSpell(Frostblast7_SpellID);

	m_Qualities.AddSpell(AcidBlast3_SpellID);
	m_Qualities.AddSpell(AcidBlast4_SpellID);
	m_Qualities.AddSpell(AcidBlast5_SpellID);
	m_Qualities.AddSpell(AcidBlast6_SpellID);
	m_Qualities.AddSpell(AcidBlast7_SpellID);

	m_Qualities.AddSpell(LightningBlast3_SpellID);
	m_Qualities.AddSpell(LightningBlast4_SpellID);
	m_Qualities.AddSpell(LightningBlast5_SpellID);
	m_Qualities.AddSpell(LightningBlast6_SpellID);
	m_Qualities.AddSpell(Lightningblast7_SpellID);

	m_Qualities.AddSpell(BladeVolley3_SpellID);
	m_Qualities.AddSpell(BladeVolley4_SpellID);
	m_Qualities.AddSpell(BladeVolley5_SpellID);
	m_Qualities.AddSpell(BladeVolley6_SpellID);
	m_Qualities.AddSpell(BladeVolley7_SpellID);

	m_Qualities.AddSpell(BludgeoningVolley3_SpellID);
	m_Qualities.AddSpell(BludgeoningVolley4_SpellID);
	m_Qualities.AddSpell(BludgeoningVolley5_SpellID);
	m_Qualities.AddSpell(BludgeoningVolley6_SpellID);
	m_Qualities.AddSpell(BludgeoningVolley7_SpellID);

	m_Qualities.AddSpell(ForceVolley3_SpellID);
	m_Qualities.AddSpell(ForceVolley4_SpellID);
	m_Qualities.AddSpell(ForceVolley5_SpellID);
	m_Qualities.AddSpell(ForceVolley6_SpellID);
	m_Qualities.AddSpell(ForceVolley7_SpellID);

	m_Qualities.AddSpell(FlameVolley3_SpellID);
	m_Qualities.AddSpell(FlameVolley4_SpellID);
	m_Qualities.AddSpell(FlameVolley5_SpellID);
	m_Qualities.AddSpell(FlameVolley6_SpellID);
	m_Qualities.AddSpell(FlameVolley7_SpellID);

	m_Qualities.AddSpell(FrostVolley3_SpellID);
	m_Qualities.AddSpell(FrostVolley4_SpellID);
	m_Qualities.AddSpell(FrostVolley5_SpellID);
	m_Qualities.AddSpell(FrostVolley6_SpellID);
	m_Qualities.AddSpell(FrostVolley7_SpellID);

	m_Qualities.AddSpell(AcidVolley3_SpellID);
	m_Qualities.AddSpell(AcidVolley4_SpellID);
	m_Qualities.AddSpell(AcidVolley5_SpellID);
	m_Qualities.AddSpell(AcidVolley6_SpellID);
	m_Qualities.AddSpell(AcidVolley7_SpellID);

	m_Qualities.AddSpell(LightningVolley3_SpellID);
	m_Qualities.AddSpell(LightningVolley4_SpellID);
	m_Qualities.AddSpell(LightningVolley5_SpellID);
	m_Qualities.AddSpell(LightningVolley6_SpellID);
	m_Qualities.AddSpell(LightningVolley7_SpellID);

	m_Qualities.AddSpell(HealthtoStaminaSelf1_SpellID);
	m_Qualities.AddSpell(HealthtoStaminaSelf2_SpellID);
	m_Qualities.AddSpell(HealthtoStaminaSelf3_SpellID);
	m_Qualities.AddSpell(HealthtoStaminaSelf4_SpellID);
	m_Qualities.AddSpell(HealthtoStaminaSelf5_SpellID);
	m_Qualities.AddSpell(HealthtoStaminaSelf6_SpellID);
	m_Qualities.AddSpell(HealthtoStaminaSelf7_SpellID);

	m_Qualities.AddSpell(HealthtoManaSelf1_SpellID);
	m_Qualities.AddSpell(HealthtoManaSelf2_SpellID);
	m_Qualities.AddSpell(HealthtoManaSelf3_SpellID);
	m_Qualities.AddSpell(HealthtoManaSelf4_SpellID);
	m_Qualities.AddSpell(HealthtoManaSelf5_SpellID);
	m_Qualities.AddSpell(HealthtoManaSelf6_SpellID);
	m_Qualities.AddSpell(HealthtoManaSelf7_SpellID);

	m_Qualities.AddSpell(StaminatoManaSelf1_SpellID);
	m_Qualities.AddSpell(StaminatoManaSelf2_SpellID);
	m_Qualities.AddSpell(StaminatoManaSelf3_SpellID);
	m_Qualities.AddSpell(StaminatoManaSelf4_SpellID);
	m_Qualities.AddSpell(StaminatoManaSelf5_SpellID);
	m_Qualities.AddSpell(StaminatoManaSelf6_SpellID);
	m_Qualities.AddSpell(StaminatoManaSelf7_SpellID);

	m_Qualities.AddSpell(StaminatoHealthSelf1_SpellID);
	m_Qualities.AddSpell(StaminatoHealthSelf2_SpellID);
	m_Qualities.AddSpell(StaminatoHealthSelf3_SpellID);
	m_Qualities.AddSpell(StaminatoHealthSelf4_SpellID);
	m_Qualities.AddSpell(StaminatoHealthSelf5_SpellID);
	m_Qualities.AddSpell(StaminatoHealthSelf6_SpellID);
	m_Qualities.AddSpell(StaminatoHealthSelf7_SpellID);

	m_Qualities.AddSpell(ManatoHealthSelf1_SpellID);
	m_Qualities.AddSpell(ManatoHealthSelf2_SpellID);
	m_Qualities.AddSpell(ManatoHealthSelf3_SpellID);
	m_Qualities.AddSpell(ManatoHealthSelf4_SpellID);
	m_Qualities.AddSpell(ManatoHealthSelf5_SpellID);
	m_Qualities.AddSpell(ManatoHealthSelf6_SpellID);
	m_Qualities.AddSpell(ManatoHealthSelf7_SpellID);

	m_Qualities.AddSpell(ManatoStaminaSelf1_SpellID);
	m_Qualities.AddSpell(ManatoStaminaSelf2_SpellID);
	m_Qualities.AddSpell(ManatoStaminaSelf3_SpellID);
	m_Qualities.AddSpell(ManatoStaminaSelf4_SpellID);
	m_Qualities.AddSpell(ManatoStaminaSelf5_SpellID);
	m_Qualities.AddSpell(ManatoStaminaSelf6_SpellID);
	m_Qualities.AddSpell(ManatoStaminaSelf7_SpellID);

	m_Qualities.AddSpell(ArmorSelf1_SpellID);
	m_Qualities.AddSpell(ArmorSelf2_SpellID);
	m_Qualities.AddSpell(ArmorSelf3_SpellID);
	m_Qualities.AddSpell(ArmorSelf4_SpellID);
	m_Qualities.AddSpell(ArmorSelf5_SpellID);
	m_Qualities.AddSpell(ArmorSelf6_SpellID);
	m_Qualities.AddSpell(ArmorSelf7_SpellID);

	m_Qualities.AddSpell(LifestoneRecall1_SpellID);
	m_Qualities.AddSpell(LifestoneTie1_SpellID);
	m_Qualities.AddSpell(PortalTieRecall1_SpellID);
	m_Qualities.AddSpell(PortalTie1_SpellID);
	m_Qualities.AddSpell(PortalTieRecall2_SpellID);
	m_Qualities.AddSpell(PortalTie2_SpellID);
	m_Qualities.AddSpell(SummonPortal1_SpellID);
	m_Qualities.AddSpell(SummonPortal2_SpellID);
	m_Qualities.AddSpell(SummonPortal3_SpellID);
	m_Qualities.AddSpell(SummonSecondPortal1_SpellID);
	m_Qualities.AddSpell(SummonSecondPortal2_SpellID);
	m_Qualities.AddSpell(SummonSecondPortal3_SpellID);

	m_Qualities.AddSpell(BloodDrinker1_SpellID);
	m_Qualities.AddSpell(BloodDrinker2_SpellID);
	m_Qualities.AddSpell(BloodDrinker3_SpellID);
	m_Qualities.AddSpell(BloodDrinker4_SpellID);
	m_Qualities.AddSpell(BloodDrinker5_SpellID);
	m_Qualities.AddSpell(BloodDrinker6_SpellID);
	m_Qualities.AddSpell(BloodDrinker7_SpellID);

	m_Qualities.AddSpell(HeartSeeker1_SpellID);
	m_Qualities.AddSpell(HeartSeeker2_SpellID);
	m_Qualities.AddSpell(HeartSeeker3_SpellID);
	m_Qualities.AddSpell(HeartSeeker4_SpellID);
	m_Qualities.AddSpell(HeartSeeker5_SpellID);
	m_Qualities.AddSpell(HeartSeeker6_SpellID);
	m_Qualities.AddSpell(Heartseeker7_SpellID);

	m_Qualities.AddSpell(SwiftKiller1_SpellID);
	m_Qualities.AddSpell(SwiftKiller2_SpellID);
	m_Qualities.AddSpell(SwiftKiller3_SpellID);
	m_Qualities.AddSpell(SwiftKiller4_SpellID);
	m_Qualities.AddSpell(SwiftKiller5_SpellID);
	m_Qualities.AddSpell(SwiftKiller6_SpellID);
	m_Qualities.AddSpell(Swiftkiller7_SpellID);

	m_Qualities.AddSpell(Defender1_SpellID);
	m_Qualities.AddSpell(Defender2_SpellID);
	m_Qualities.AddSpell(Defender3_SpellID);
	m_Qualities.AddSpell(Defender4_SpellID);
	m_Qualities.AddSpell(Defender5_SpellID);
	m_Qualities.AddSpell(Defender6_SpellID);
	m_Qualities.AddSpell(Defender7_SpellID);

#ifndef PUBLIC_BUILD
	if (IsAdmin())
	{
		for (DWORD i = 0; i < 8000; i++)
		{
			/*
			const CSpellBase *spell = CachedSpellTable->GetSpellBase(i);
			if (spell)
			{
			if (spell->_meta_spell._sp_type == Enchantment_SpellType)
			m_Qualities.AddSpell(i);
			}
			*/
			m_Qualities.AddSpell(i);
		}

		m_Qualities.AddSpell(BunnySmite_SpellID);
		m_Qualities.AddSpell(ImperilOther7_SpellID);
		m_Qualities.AddSpell(ArmorOther7_SpellID);
		m_Qualities.AddSpell(ArmorSelf7_SpellID);
		m_Qualities.AddSpell(FlameBlast3_SpellID);
		m_Qualities.AddSpell(FlameVolley3_SpellID);
		m_Qualities.AddSpell(FlameWall_SpellID);
		m_Qualities.AddSpell(FlameRing_SpellID);

		m_Qualities.AddSpell(AerfallesWard_SpellID);
		m_Qualities.AddSpell(Impulse_SpellID);
		m_Qualities.AddSpell(BunnySmite_SpellID);
		m_Qualities.AddSpell(BaelZharonSmite_SpellID);
		m_Qualities.AddSpell(CrystalSunderRing_SpellID);
		m_Qualities.AddSpell(RecallAsmolum1_SpellID);
		m_Qualities.AddSpell(ShadowCloudManaDrain_SpellID);
		m_Qualities.AddSpell(ShadowCloudLifeDrain_SpellID);
		m_Qualities.AddSpell(SanctuaryRecall_SpellID);
		m_Qualities.AddSpell(RecallAsmolum2_SpellID);
		m_Qualities.AddSpell(RecallAsmolum3_SpellID);
		m_Qualities.AddSpell(ShadowCloudStamDrain_SpellID);
		m_Qualities.AddSpell(Martyr_SpellID);
		m_Qualities.AddSpell(SummonPortalCoIPK_SpellID);
		m_Qualities.AddSpell(StaminaBlight_SpellID);
		m_Qualities.AddSpell(FlamingBlaze_SpellID);
		m_Qualities.AddSpell(SteelThorns_SpellID);
		m_Qualities.AddSpell(ElectricBlaze_SpellID);
		m_Qualities.AddSpell(AcidicSpray_SpellID);
		m_Qualities.AddSpell(ExplodingFury_SpellID);
		m_Qualities.AddSpell(ElectricDischarge_SpellID);
		m_Qualities.AddSpell(FumingAcid_SpellID);
		m_Qualities.AddSpell(FlamingIrruption_SpellID);
		m_Qualities.AddSpell(ExplodingIce_SpellID);
		m_Qualities.AddSpell(SparkingFury_SpellID);
		m_Qualities.AddSpell(SummonPortalHopeslayer_SpellID);
		m_Qualities.AddSpell(RecallAerlinthe_SpellID);
		m_Qualities.AddSpell(BaelzharonWallFire_SpellID);
		m_Qualities.AddSpell(BaelzharonWeaknessOther_SpellID);
		m_Qualities.AddSpell(BaelzharonItemIneptOther_SpellID);
		m_Qualities.AddSpell(BaelzharonRainBludgeon_SpellID);
		m_Qualities.AddSpell(BaelzharonPortalExile_SpellID);
		m_Qualities.AddSpell(BaelzharonArmorOther_SpellID);
		m_Qualities.AddSpell(BaelzharonMagicDefense_SpellID);
		m_Qualities.AddSpell(BaelzharonBloodDrinker_SpellID);
		m_Qualities.AddSpell(WeddingSteele_SpellID);
		m_Qualities.AddSpell(SteeleGenericFancy_SpellID);
	}
#endif
}

void CPlayerWeenie::UpdateModuleFromClient(PlayerModule &module)
{
	bool bOldShowHelm = ShowHelm();

	_playerModule = module;

	if (bOldShowHelm != ShowHelm())
	{
		UpdateModel();
	}
}

void CPlayerWeenie::LoadEx(CWeenieSave &save)
{
	CContainerWeenie::LoadEx(save);

	if (save._playerModule)
		_playerModule = *save._playerModule;

	if (save._questTable)
		_questTable = *save._questTable;

	CheckVitalRanges();
}

void CPlayerWeenie::SaveEx(CWeenieSave &save)
{
	DebugValidate();

	CContainerWeenie::SaveEx(save);

	SafeDelete(save._playerModule);
	save._playerModule = new PlayerModule;
	*save._playerModule = _playerModule;

	SafeDelete(save._questTable);
	save._questTable = new QuestTable;
	*save._questTable = _questTable;
}

bool CPlayerWeenie::ShowHelm()
{
	if (_playerModule.options2_ & ShowHelm_CharacterOptions2)
		return true;

	return false;
}

void CPlayerWeenie::TryStackableMerge(DWORD merge_from_id, DWORD merge_to_id, DWORD amount)
{
	CWeenieObject *pSource = FindContainedItem(merge_from_id);
	CWeenieObject *pTarget = FindContainedItem(merge_to_id);

	if (!pTarget)
	{
		NotifyInventoryFailedEvent(merge_from_id, WERROR_OBJECT_GONE);
		return;
	}

	if (!pSource)
	{
		// maybe it's on the ground
		pSource = g_pWorld->FindObject(merge_from_id);

		if (!pSource || pSource->HasOwner() || !pSource->InValidCell())
		{
			NotifyInventoryFailedEvent(merge_from_id, WERROR_OBJECT_GONE);
			return;
		}

		if (DistanceTo(pSource, true) > 2.0)
		{
			// TODO move towards object instead
			NotifyInventoryFailedEvent(merge_from_id, WERROR_TOO_FAR);
			return;
		}
	}

	if (pSource->m_Qualities.id != pTarget->m_Qualities.id)
	{
		NotifyInventoryFailedEvent(merge_from_id, WERROR_NONE);
		return;
	}

	DWORD source_current_stack = pSource->InqIntQuality(STACK_SIZE_INT, 1);
	if (amount > source_current_stack)
	{
		NotifyInventoryFailedEvent(merge_from_id, WERROR_NONE);
		return;
	}

	DWORD target_current_stack = pTarget->InqIntQuality(STACK_SIZE_INT, 1);
	DWORD target_max_stack = pTarget->InqIntQuality(MAX_STACK_SIZE_INT, 0);
	int target_stack_room = target_max_stack - target_current_stack;

	if (target_stack_room <= 0)
	{
		NotifyInventoryFailedEvent(merge_from_id, WERROR_NONE);
		return;
	}

	if (target_stack_room < (int) amount)
	{
		amount = target_stack_room;
	}
	
	DWORD target_new_stack_size = target_current_stack + amount;
	pTarget->SetStackSize(target_new_stack_size);

	DWORD source_new_stack_size = source_current_stack - amount;
	if (source_new_stack_size > 0)
	{
		pSource->SetStackSize(source_new_stack_size);

		BinaryWriter sourceSetStackSize;
		sourceSetStackSize.Write<DWORD>(0x197);
		sourceSetStackSize.Write<BYTE>(pSource->GetNextStatTimestamp(Int_StatType, STACK_SIZE_INT)); // FIXME TODO not sure sequence should be from item or the player
		sourceSetStackSize.Write<DWORD>(pSource->GetID());
		sourceSetStackSize.Write<DWORD>(pSource->InqIntQuality(STACK_SIZE_INT, 1));
		sourceSetStackSize.Write<DWORD>(pSource->InqIntQuality(VALUE_INT, 0));
		SendNetMessage(&sourceSetStackSize, PRIVATE_MSG, FALSE, FALSE);
	}
	else
	{
		CWeenieObject *owner = pSource->GetWorldTopLevelOwner();
		if (owner)
		{
			owner->ReleaseContainedItemRecursive(pSource);

			BinaryWriter removeInventoryObjectMessage;
			removeInventoryObjectMessage.Write<DWORD>(0x24);
			removeInventoryObjectMessage.Write<DWORD>(pSource->GetID());
			owner->SendNetMessage(&removeInventoryObjectMessage, PRIVATE_MSG, FALSE, FALSE);
		}

		pSource->MarkForDestroy();
	}

	BinaryWriter targetSetStackSize;
	targetSetStackSize.Write<DWORD>(0x197);
	targetSetStackSize.Write<BYTE>(pTarget->GetNextStatTimestamp(Int_StatType, STACK_SIZE_INT)); // FIXME TODO not sure sequence should be from item or the player
	targetSetStackSize.Write<DWORD>(pTarget->GetID());
	targetSetStackSize.Write<DWORD>(pTarget->InqIntQuality(STACK_SIZE_INT, 1));
	targetSetStackSize.Write<DWORD>(pTarget->InqIntQuality(VALUE_INT, 0));
	SendNetMessage(&targetSetStackSize, PRIVATE_MSG, FALSE, FALSE);

	RecalculateEncumbrance();
}

void CPlayerWeenie::TryStackableSplitToContainer(DWORD stack_id, DWORD container_id, DWORD place, DWORD amount)
{
	CWeenieObject *pSource = FindContainedItem(stack_id);
	if (!pSource)
	{
		NotifyInventoryFailedEvent(stack_id, WERROR_OBJECT_GONE);
		return;
	}

	CContainerWeenie *pTarget = FindContainer(container_id); // should really use any visible container
	if (!pTarget)
	{
		NotifyInventoryFailedEvent(stack_id, WERROR_OBJECT_GONE);
		return;
	}

	if (pSource->AsContainer())
	{
		NotifyInventoryFailedEvent(stack_id, WERROR_OBJECT_GONE); // can't store containers... or split them for that matter
		return;
	}

	if (pTarget->IsItemsCapacityFull())
	{
		NotifyInventoryFailedEvent(stack_id, WERROR_FULL_INVENTORY_LOCATION);
		return;
	}

	// TODO not done
	NotifyInventoryFailedEvent(stack_id, WERROR_OBJECT_GONE);

	RecalculateEncumbrance();
}

void CPlayerWeenie::TryStackableSplitTo3D(DWORD stack_id, DWORD amount)
{
	// TODO not done
	NotifyInventoryFailedEvent(stack_id, WERROR_OBJECT_GONE);

	RecalculateEncumbrance();
}

void CPlayerWeenie::TryStackableSplitToWield(DWORD stack_id, DWORD loc, DWORD amount)
{
	// TODO not done
	NotifyInventoryFailedEvent(stack_id, WERROR_OBJECT_GONE);

	RecalculateEncumbrance();
}

bool CPlayerWeenie::InqQuest(const char *questName)
{
	return _questTable.InqQuest(questName);
}

int CPlayerWeenie::InqTimeUntilOkayToComplete(const char *questName)
{
	return _questTable.InqTimeUntilOkayToComplete(questName);
}

unsigned int CPlayerWeenie::InqQuestSolves(const char *questName)
{
	return _questTable.InqQuestSolves(questName);
}

bool CPlayerWeenie::UpdateQuest(const char *questName)
{
	return _questTable.UpdateQuest(questName);
}

void CPlayerWeenie::StampQuest(const char *questName)
{
	_questTable.StampQuest(questName);
}

void CPlayerWeenie::IncrementQuest(const char *questName)
{
	_questTable.IncrementQuest(questName);
}

void CPlayerWeenie::DecrementQuest(const char *questName)
{
	_questTable.DecrementQuest(questName);
}

void CPlayerWeenie::EraseQuest(const char *questName)
{
	_questTable.RemoveQuest(questName);
}

void CLifestoneRecallUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(0x10000153); // Motion_LifestoneRecall - animation shifted
	_weenie->SendText(csprintf("%s is recalling to the lifestone.", _weenie->GetName().c_str()), LTT_RECALL);
}

void CLifestoneRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	_weenie->TeleportToLifestone();
	Done();
}

void CMarketplaceRecallUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(0x10000166); // Motion_MarketplaceRecall - animation shifted
	_weenie->SendText(csprintf("%s is going to the Marketplace.", _weenie->GetName().c_str()), LTT_RECALL);
}

void CMarketplaceRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace())
	{
		Cancel();
		return;
	}

	_weenie->Movement_Teleport(Position(0x016C01BC, Vector(49.11f, -31.22f, 0.005f), Quaternion(0.7009f, 0, 0, -0.7132f)));
	Done();
}

void CPlayerWeenie::BeginRecall(const Position &targetPos)
{
	EmitEffect(PS_Hide, 1.0f);

	_recallTime = Timer::cur_time + 2.0;
	_recallPos = targetPos;
}

void CPlayerWeenie::OnTeleported()
{
	CWeenieObject::OnTeleported();
	_recallTime = -1.0; // cancel any teleport
}