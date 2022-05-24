/* See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "dragger_beam.h"
#include "character.h"
#include "dragger.h"
#include <engine/config.h>
#include <engine/server.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/player.h>
#include <game/server/teams.h>

CDraggerBeam::CDraggerBeam(CGameWorld *pGameWorld, CDragger *pDragger, vec2 Pos, float Strength, bool IgnoreWalls,
	int ForClientID) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_pDragger = pDragger;
	m_Pos = Pos;
	m_Strength = Strength;
	m_IgnoreWalls = IgnoreWalls;
	m_ForClientID = ForClientID;
	m_Active = true;
	m_EvalTick = Server()->Tick();

	GameWorld()->InsertEntity(this);
}

void CDraggerBeam::Tick()
{
	if(!m_Active)
	{
		return;
	}

	// Drag only if the player is reachable and alive
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_ForClientID);
	if(!pTarget)
	{
		Reset();
		return;
	}

	int IsReachable =
		m_IgnoreWalls ?
			!GameServer()->Collision()->IntersectNoLaserNW(m_Pos, pTarget->m_Pos, 0, 0) :
			!GameServer()->Collision()->IntersectNoLaser(m_Pos, pTarget->m_Pos, 0, 0);
	// This check is necessary because the check in CDragger::LookForPlayersToDrag only happens every 150ms
	if(!IsReachable ||
		distance(pTarget->m_Pos, m_Pos) >= g_Config.m_SvDraggerRange || !pTarget->IsAlive())
	{
		Reset();
		return;
	}
	// In the center of the dragger a tee does not experience speed-up
	else if(distance(pTarget->m_Pos, m_Pos) > 28)
	{
		vec2 Temp = pTarget->Core()->m_Vel + (normalize(m_Pos - pTarget->m_Pos) * m_Strength);
		pTarget->Core()->m_Vel = ClampVel(pTarget->m_MoveRestrictions, Temp);
	}
}

void CDraggerBeam::SetPos(vec2 Pos)
{
	m_Pos = Pos;
}

void CDraggerBeam::Reset()
{
	m_MarkedForDestroy = true;
	m_Active = false;

	m_pDragger->RemoveDraggerBeam(m_ForClientID);
}

void CDraggerBeam::Snap(int SnappingClient)
{
	if(!m_Active)
	{
		return;
	}

	// Only players who can see the player attached to the dragger can see the dragger beam
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_ForClientID);
	if(!pTarget->CanSnapCharacter(SnappingClient))
	{
		return;
	}
	// Only players with the dragger beam in their field of view or who want to see everything will receive the snap
	vec2 TargetPos = vec2(pTarget->m_Pos.x, pTarget->m_Pos.y);
	if(NetworkClippedLine(SnappingClient, m_Pos, TargetPos))
	{
		return;
	}
	CNetObj_Laser *obj = static_cast<CNetObj_Laser *>(
		Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!obj)
	{
		return;
	}
	obj->m_X = (int)m_Pos.x;
	obj->m_Y = (int)m_Pos.y;
	obj->m_FromX = (int)TargetPos.x;
	obj->m_FromY = (int)TargetPos.y;

	int StartTick = m_EvalTick;
	if(StartTick < Server()->Tick() - 4)
	{
		StartTick = Server()->Tick() - 4;
	}
	else if(StartTick > Server()->Tick())
	{
		StartTick = Server()->Tick();
	}
	obj->m_StartTick = StartTick;
}
