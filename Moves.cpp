
#include "StdAfx.h"
#include "WeenieObject.h"
#include "PhysicsObj.h"
#include "World.h"

#include "ObjectMsgs.h"

void CPhysicsObj::Movement_Init()
{
	_next_move_think = 0.0;
}

void CPhysicsObj::Movement_Shutdown()
{
}

void CPhysicsObj::Movement_Think()
{
	if (parent)
		return;

	if (_next_move_think < Timer::cur_time)
	{
		if (m_Position.objcell_id != _last_move_position.objcell_id || !m_Position.frame.is_vector_equal(_last_move_position.frame))
		{
			Movement_UpdatePos();
			// Animation_Update();
		}
		else if (!m_Position.frame.is_quaternion_equal(_last_move_position.frame))
		{
			Movement_UpdatePos();
		}

		_next_move_think = Timer::cur_time + 1.0;
	}
}

void CPhysicsObj::Movement_SendUpdate(DWORD dwCell)
{
	if (CWeenieObject *pWeenie = GetWeenie())
	{
		BinaryWriter* poo = MoveUpdate(pWeenie);
		g_pWorld->BroadcastPVS(dwCell, poo->GetData(), poo->GetSize());
		delete poo;
	}
}

void CPhysicsObj::Movement_UpdatePos()
{
	if (parent)
		return;

	//QUICKFIX: Broadcast to the old landblock that we've moved from.
	//This sends duplicates if the block is near the other.

	_position_timestamp++;
	_last_update_pos = Timer::cur_time;

	DWORD dwNewCell = GetLandcell();
	DWORD dwOldCell = _last_move_position.objcell_id;

	if (BLOCK_WORD(dwOldCell) != BLOCK_WORD(dwNewCell))
	{
		Movement_SendUpdate(dwOldCell);
	}

	Movement_SendUpdate(dwNewCell);

	_last_move_position = m_Position;

	/*
	GetWeenie()->EmoteLocal(csprintf("Sending position update. Pos: %.1f %.1f %.1f v: %.1f %.1f %.1f", 
		m_Position.frame.m_origin.x, m_Position.frame.m_origin.y, m_Position.frame.m_origin.z,
		m_velocityVector.x, m_velocityVector.y, m_velocityVector.z));
		*/
}


void CPhysicsObj::Movement_UpdateVector()
{
	if (parent)
		return;

	BinaryWriter moveMsg;
	moveMsg.Write<DWORD>(0xF74E);
	moveMsg.Write<DWORD>(id);

	// velocity
	Vector localVel = m_velocityVector;
	localVel.Pack(&moveMsg);

	// omega
	m_Omega.Pack(&moveMsg);

	moveMsg.Write<WORD>(_instance_timestamp);
	moveMsg.Write<WORD>(++_vector_timestamp);

	g_pWorld->BroadcastPVS(this, moveMsg.GetData(), moveMsg.GetSize(), OBJECT_MSG);
}
