/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"

enum binoculars_e
{
	BINOCULARS_IDLE1 = 0,
	BINOCULARS_IDLE2,
	BINOCULARS_IDLE3,
	BINOCULARS_SHOOT,
	BINOCULARS_SHOOT_EMPTY,
	BINOCULARS_RELOAD,
	BINOCULARS_RELOAD_NOT_EMPTY,
	BINOCULARS_DRAW,
	BINOCULARS_HOLSTER,
	BINOCULARS_ADD_SILENCER
};

static int g_nZoomFOV[] =
{
	0,
	40,
	30,
	20,
	10,
	5
};

LINK_ENTITY_TO_CLASS( weapon_binoculars, CBinoculars )

void CBinoculars::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_binoculars" ); // hack to allow for old names
	Precache();
	m_iId = WEAPON_BINOCULARS;
	SET_MODEL( ENT( pev ), "models/w_9mmhandgun.mdl" );

	FallInit();// get ready to fall down.
}

void CBinoculars::Precache( void )
{
	PRECACHE_MODEL( "models/v_9mmhandgun.mdl" );
	PRECACHE_MODEL( "models/w_9mmhandgun.mdl" );
	PRECACHE_MODEL( "models/p_9mmhandgun.mdl" );

	PRECACHE_SOUND( "weapons/binoculars/binoculars_zoomin.wav" );
	PRECACHE_SOUND( "weapons/binoculars/binoculars_zoomout.wav" );
	PRECACHE_SOUND( "weapons/binoculars/binoculars_zoommax.wav" );
}

int CBinoculars::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_BINOCULARS;
	p->iWeight = -30;

	return 1;
}

int CBinoculars::AddToPlayer( CBasePlayer *pPlayer )
{
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CBinoculars::Deploy()
{
	// pev->body = 1;
	m_nZoomLevel = 0;
	
	Zoom();
	
	return DefaultDeploy( "models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", BINOCULARS_DRAW, "trip", 0 );
}

void CBinoculars::Holster( int skiplocal /* = 0 */ )
{
	m_nZoomLevel = 0;
	
	Zoom();

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	
	SendWeaponAnim( BINOCULARS_HOLSTER );
}

void CBinoculars::PrimaryAttack( void )
{
	if( m_nZoomLevel == 5 )
	{
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/binoculars/binoculars_zoommax.wav", 1, ATTN_NORM );
	}
	else
	{
		m_nZoomLevel += 1;
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/binoculars/binoculars_zoomin.wav", 1, ATTN_NORM );
	}
	
	Zoom();
	
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25f;
}

void CBinoculars::SecondaryAttack( void )
{		
	if( m_nZoomLevel == 0 )
	{
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/binoculars/binoculars_zoommax.wav", 1, ATTN_NORM );
	}
	else
	{
		m_nZoomLevel -= 1;
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/binoculars/binoculars_zoomout.wav", 1, ATTN_NORM );
	}
	
	Zoom();
	
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25f;
}

void CBinoculars::Zoom( void )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = g_nZoomFOV[m_nZoomLevel];
}	

void CBinoculars::WeaponIdle( void )
{
	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

	if( flRand <= 0.3f + 0 * 0.75f )
	{
		iAnim = BINOCULARS_IDLE3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0f / 16.0f;
	}
	else if( flRand <= 0.6f + 0 * 0.875f )
	{
		iAnim = BINOCULARS_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0f / 16.0f;
	}
	else
	{
		iAnim = BINOCULARS_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0f / 16.0f;
	}
	SendWeaponAnim( iAnim, 1 );
}
