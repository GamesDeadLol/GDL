
#pragma once

#include "Container.h"
#include "PhysicsObj.h"

enum MotionUseType
{
	MUT_UNDEF = 0,
	MUT_CONSUME_FOOD,
};

struct MotionUseData
{
	void Reset()
	{
		m_MotionUseType = MUT_UNDEF;
	}

	int m_MotionUseType = MUT_UNDEF;
	DWORD m_MotionUseMotionID = 0;
	DWORD m_MotionUseTarget = 0;
	DWORD m_MotionUseChildID = 0;
	DWORD m_MotionUseChildLocation = 0;
};

class CMonsterWeenie : public CContainerWeenie
{
public:
	CMonsterWeenie();
	virtual ~CMonsterWeenie() override;

	virtual class CMonsterWeenie *AsMonster() { return this; }

	virtual void Tick();

	static bool ClothingPrioritySorter(const CWeenieObject *first, const CWeenieObject *second);
	virtual void GetObjDesc(ObjDesc &objDesc) override;

	virtual void ApplyQualityOverrides() override;

	virtual void OnDeathAnimComplete();
	virtual void OnDeath(DWORD killer_id) override;
	virtual void OnMotionDone(DWORD motion, BOOL success) override;

	virtual void OnTookDamage(DamageEventData &damageData) override;

	DWORD DoForcedUseMotion(MotionUseType useType, DWORD motion, DWORD target = 0, DWORD childID = 0, DWORD childLoc = 0, MovementParameters *params = NULL);

	virtual void PreSpawnCreate() override;
	virtual void PostSpawn() override;

	virtual void TryMeleeAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion = 0) override;
	virtual void TryMissileAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion = 0) override;

	virtual bool IsDead() override;

	virtual double GetDefenseMod() override;
	virtual double GetDefenseModUsingWielded() override;
	virtual double GetOffenseMod() override;
	virtual int GetAttackTime() override;
	virtual int GetAttackTimeUsingWielded() override;
	virtual int GetAttackDamage() override;
	virtual float GetEffectiveArmorLevel(DamageEventData &damageData, bool bIgnoreMagicArmor) override;

	virtual void OnIdentifyAttempted(CWeenieObject *other) override;
	virtual void HandleAggro(CWeenieObject *attacker) override;
	virtual void ResistedSpell(CWeenieObject *attacker) override;
	virtual void EvadedAttack(CWeenieObject *attacker) override;
	
	void DropAllLoot(CCorpseWeenie *pCorpse);
	virtual void GenerateDeathLoot(CCorpseWeenie *pCorpse);

	virtual BOOL DoCollision(const class ObjCollisionProfile &prof);

	CCorpseWeenie *CreateCorpse();

	bool IsAttackMotion(DWORD motion);

	DWORD m_LastAttackTarget = 0;
	DWORD m_LastAttackHeight = 1;
	float m_LastAttackPower = 0.0f;

	DWORD m_AttackAnimTarget = 0;
	DWORD m_AttackAnimHeight = 1;
	float m_AttackAnimPower = 0.0f;

	bool m_bChargingAttack = false;
	DWORD m_ChargingAttackTarget = 0;
	DWORD m_ChargingAttackHeight = false;
	float m_ChargingAttackPower = 0.0f;
	float m_fChargeAttackStartTime = (float) INVALID_TIME;

	unsigned int m_MeleeDamageBonus = 0;
	
	bool TryEquipItem(CWeenieObject *item_weenie, DWORD inv_location);
	bool TryEquipItem(DWORD dwItemID, DWORD dwCoverage);

	virtual void ChangeCombatMode(COMBAT_MODE mode, bool playerRequested) override;

	virtual DWORD ReceiveInventoryItem(CWeenieObject *source, CWeenieObject *item, DWORD desired_slot) override;

	class MonsterAIManager *m_MonsterAI = NULL;

private:
	CWeenieObject *SpawnWielded(DWORD wcid, int ptid, float shade);
	void CheckRegeneration(bool &bRegenerateNext, double &lastRegen, float regenRate, STypeAttribute2nd currentAttrib, STypeAttribute2nd maxAttrib);

	bool m_bRegenHealthNext = false;
	double m_fLastHealthRegen = 0.0;

	bool m_bRegenStaminaNext = false;
	double m_fLastStaminaRegen = 0.0;

	bool m_bRegenManaNext = false;
	double m_fLastManaRegen = 0.0;

	bool m_bWaitingForDeathToFinish = false;
	std::string m_DeathKillerNameForCorpse;
	DWORD m_DeathKillerIDForCorpse;

	MotionUseData m_MotionUseData;
};

/*
class CBaelZharon : public CMonsterWeenie
{
public:
	CBaelZharon();

	BOOL CrazyThink();
};

class CTargetDrudge : public CMonsterWeenie
{
public:
	CTargetDrudge();
};
*/
