
#include "StdAfx.h"
#include "World.h"
#include "GameMode.h"
#include "Player.h"
#include "WeenieObject.h"
#include "ChatMsgs.h"

CGameMode::CGameMode()
{
}

CGameMode::~CGameMode()
{
}

CGameMode_Tag::CGameMode_Tag()
{
	_selectedPlayer = NULL;
}

CGameMode_Tag::~CGameMode_Tag()
{
	UnselectPlayer();
}

const char *CGameMode_Tag::GetName()
{
	return "Tag";
}

void CGameMode_Tag::Think()
{
	if (!_selectedPlayer)
	{
		// Find a player to make "it."
		PlayerWeenieMap *pPlayers = g_pWorld->GetPlayers();

		if (pPlayers->size() < 2)
		{
			return;
		}

		int index = Random::GenUInt(0, (unsigned int )(pPlayers->size() - 1));

		CPlayerWeenie *pSelected = NULL;
		int i = 0;

		for (auto& player : *pPlayers)
		{
			if (i == index)
			{
				pSelected = player.second;
				break;
			}

			i++;
		}

		SelectPlayer(pSelected);
	}
}

void CGameMode_Tag::SelectPlayer(CPlayerWeenie *pPlayer)
{
	if (!pPlayer)
	{
		UnselectPlayer();
		return;
	}

	_selectedPlayer = pPlayer;

	_selectedPlayer->EmitEffect(PS_HealthDownRed, 1.0f);
	g_pWorld->BroadcastGlobal(ServerText(csprintf("%s is it!", _selectedPlayer->GetName().c_str()), LTT_DEFAULT), PRIVATE_MSG);
}

void CGameMode_Tag::UnselectPlayer()
{
	if (!_selectedPlayer)
	{
		return;
	}
}

void CGameMode_Tag::OnTargetAttacked(CWeenieObject *pTarget, CWeenieObject *pSource)
{
	if (pSource == _selectedPlayer)
	{
		if (CPlayerWeenie *pTargetPlayer = pTarget->AsPlayer())
		{
			UnselectPlayer();
			SelectPlayer(pTargetPlayer);
		}
	}
}

void CGameMode_Tag::OnRemoveEntity(CWeenieObject *pEntity)
{
	if (pEntity)
	{
		if (pEntity == _selectedPlayer)
		{
			UnselectPlayer();
			_selectedPlayer = NULL;
		}
	}
}
