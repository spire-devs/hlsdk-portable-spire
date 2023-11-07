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
#if !OEM_BUILD

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

enum ml_e
{
	ML_IDLE = 0,
	ML_FIDGET,
	ML_RELOAD,		// to reload
	ML_FIRE2,		// to empty
	ML_HOLSTER1,	// loaded
	ML_DRAW1,		// loaded
	ML_HOLSTER2,	// unloaded
	ML_DRAW_UL,	// unloaded
	ML_IDLE_UL,	// unloaded idle
	ML_FIDGET_UL	// unloaded fidget
};

LINK_ENTITY_TO_CLASS( weapon_ml, CMl )

#if !CLIENT_DLL

LINK_ENTITY_TO_CLASS( ml_rocket, CMlRocket )

//=========================================================
//=========================================================
CMlRocket *CMlRocket::CreateMlRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CMl *pLauncher )
{
	CMlRocket *pRocket = GetClassPtr( (CMlRocket *)NULL );

	UTIL_SetOrigin( pRocket, vecOrigin );
	pRocket->pev->angles = vecAngles;
	pRocket->Spawn();
	pRocket->SetTouch( &CMlRocket::RocketTouch );
	pRocket->m_hLauncher = pLauncher;// remember what ML fired me. 
	pLauncher->m_cActiveRockets++;// register this missile as active for the launcher
	pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

//=========================================================
//=========================================================
void CMlRocket::Spawn( void )
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/rpgrocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( this, pev->origin );

	pev->classname = MAKE_STRING( "ml_rocket" );

	SetThink( &CMlRocket::IgniteThink );
	SetTouch(&CMlRocket :: ExplodeTouch );

	pev->angles.x -= 30.0f;
	UTIL_MakeVectors( pev->angles );
	pev->angles.x = -( pev->angles.x + 30.0f );

	pev->velocity = gpGlobals->v_forward * 250.0f;
	pev->gravity = 0.5f;

	SetNextThink( 0.4f );

	pev->dmg = gSkillData.plrDmgRPG;
}

//=========================================================
//=========================================================
void CMlRocket::RocketTouch( CBaseEntity *pOther )
{
	if( CMl* pLauncher = (CMl*)( (CBaseEntity*)( m_hLauncher ) ) )
	{
		// my launcher is still around, tell it I'm dead.
		pLauncher->m_cActiveRockets--;
	}

	STOP_SOUND( edict(), CHAN_VOICE, "weapons/stinger/stinger_flyloop.wav" );
	ExplodeTouch( pOther );
}

//=========================================================
//=========================================================
void CMlRocket::Precache( void )
{
	PRECACHE_MODEL( "models/rpgrocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );
	PRECACHE_SOUND( "weapons/stinger/stinger_flyloop.wav" );
}

void CMlRocket::IgniteThink( void )
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/stinger/stinger_flyloop.wav", 1, 0.5f );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink( &CMlRocket::FollowThink );
	SetNextThink( 0.1f );
}

void CMlRocket::FollowThink( void )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors( pev->angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;
	
	// Examine all entities within a reasonable radius
	while( ( pOther = UTIL_FindEntityByClassname( pOther, "laser_spot" ) ) != NULL )
	{
		UTIL_TraceLine( pev->origin, pOther->pev->origin, dont_ignore_monsters, ENT( pev ), &tr );
		// ALERT( at_console, "%f\n", tr.flFraction );
		if( tr.flFraction >= 0.9f )
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length();
			vecDir = vecDir.Normalize();
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if( ( flDot > 0 ) && ( flDist * ( 1 - flDot ) < flMax ) )
			{
				flMax = flDist * ( 1 - flDot );
				vecTarget = vecDir;
			}
		}
	}

	pev->angles = UTIL_VecToAngles( vecTarget );

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if( gpGlobals->time - m_flIgniteTime < 1.0f )
	{
		pev->velocity = pev->velocity * 0.2f + vecTarget * ( flSpeed * 0.8f + 400 );
		if( pev->waterlevel == 3 && pev->watertype > CONTENT_FLYFIELD )
		{
			// go slow underwater
			if( pev->velocity.Length() > 300.0f )
			{
				pev->velocity = pev->velocity.Normalize() * 300.0f;
			}
			UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1f, pev->origin, 4 );
		} 
		else 
		{
			if( pev->velocity.Length() > 2000.0f )
			{
				pev->velocity = pev->velocity.Normalize() * 2000.0f;
			}
		}
	}
	else
	{
		if( pev->effects & EF_LIGHT )
		{
			pev->effects = 0;
			STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/stinger/stinger_flyloop.wav" );
		}
		pev->velocity = pev->velocity * 0.2f + vecTarget * flSpeed * 0.798f;
		if( ( pev->waterlevel == 0 || pev->watertype == CONTENT_FOG ) && pev->velocity.Length() < 1500.0f )
		{
			if( CMl *pLauncher = (CMl*)( (CBaseEntity*)( m_hLauncher ) ) )
			{
				// my launcher is still around, tell it I'm dead.
				pLauncher->m_cActiveRockets--;
			}
			Detonate();
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	SetNextThink( 0.1f );
}
#endif

void CMl::Reload( void )
{
	int iResult = 0;

	// don't bother with any of this if don't need to reload.
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == ML_MAX_CLIP )
		return;

	// because the ML waits to autoreload when no missiles are active while  the LTD is on, the
	// weapons code is constantly calling into this function, but is often denied because 
	// a) missiles are in flight, but the LTD is on
	// or
	// b) player is totally out of ammo and has nothing to switch to, and should be allowed to
	//    shine the designator around
	//
	// Set the next attack time into the future so that WeaponIdle will get called more often
	// than reload, allowing the ML LTD to be updated
	
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );

	if( m_cActiveRockets && m_fSpotActive )
	{
		// no reloading when there are active missiles tracking the designator.
		// ward off future autoreload attempts by setting next attack time into the future for a bit. 
		return;
	}

#if !CLIENT_DLL
	if( m_pSpot && m_fSpotActive )
	{
		m_pSpot->Suspend( 2.1f );
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.1f;
	}
#endif

	if( m_iClip == 0 )
		iResult = DefaultReload( ML_MAX_CLIP, ML_RELOAD, 2 );

	if( iResult )
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CMl::Spawn()
{
	Precache();
	m_iId = WEAPON_ML;

	SET_MODEL( ENT( pev ), "models/w_rpg.mdl" );
	m_fSpotActive = 1;

#if CLIENT_DLL
	if( bIsMultiplayer() )
#else
	if( g_pGameRules->IsMultiplayer() )
#endif
	{
		// more default ammo in multiplay. 
		m_iDefaultAmmo = ML_DEFAULT_GIVE * 2;
	}
	else
	{
		m_iDefaultAmmo = ML_DEFAULT_GIVE;
	}

	FallInit();// get ready to fall down.
}

void CMl::Precache( void )
{
	PRECACHE_MODEL( "models/w_rpg.mdl" );
	PRECACHE_MODEL( "models/v_rpg.mdl" );
	PRECACHE_MODEL( "models/p_rpg.mdl" );

	PRECACHE_SOUND( "items/9mmclip1.wav" );

	UTIL_PrecacheOther( "laser_spot" );
	UTIL_PrecacheOther( "ml_rocket" );

	PRECACHE_SOUND( "weapons/stinger/stinger_fire.wav" );

	m_usMl = PRECACHE_EVENT( 1, "events/ml.sc" );
}

int CMl::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = ML_MAX_CLIP;
	p->iSlot = 4;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_ML;
	p->iFlags = 0;
	p->iWeight = ML_WEIGHT;

	return 1;
}

int CMl::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CMl::Deploy()
{
	if( m_iClip == 0 )
	{
		return DefaultDeploy( "models/v_rpg.mdl", "models/p_rpg.mdl", ML_DRAW_UL, "rpg" );
	}

	return DefaultDeploy( "models/v_rpg.mdl", "models/p_rpg.mdl", ML_DRAW1, "rpg" );
}

BOOL CMl::CanHolster( void )
{
	if( m_fSpotActive && m_cActiveRockets )
	{
		// can't put away while guiding a missile.
		return FALSE;
	}

	return TRUE;
}

void CMl::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	SendWeaponAnim( ML_HOLSTER1 );

#if !CLIENT_DLL
	if( m_pSpot )
	{
		m_pSpot->Killed( NULL, GIB_NEVER );
		m_pSpot = NULL;
	}
#endif
}

void CMl::PrimaryAttack()
{
	if( m_iClip )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#if !CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 16.0f + gpGlobals->v_right * 8.0f + gpGlobals->v_up * -8.0f;

		CMlRocket *pRocket = CMlRocket::CreateMlRocket( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer, this );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );// MlRocket::Create stomps on globals, so remake.
		pRocket->pev->velocity = pRocket->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
#endif

		// firing ML no longer turns on the designator. ALT fire is a toggle switch for the LTD.
		// Ken signed up for this as a global change (sjb)

		// make rocket sound
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/stinger/stinger_fire.wav", 1, 0.5f );
		
		int flags;
#if CLIENT_WEAPONS
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usMl );

		m_iClip--; 

		m_flNextPrimaryAttack = GetNextAttackDelay( 1.5f );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5f;

		ResetEmptySound();
	}
	else
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2f;
	}
	UpdateSpot();
}

void CMl::SecondaryAttack()
{
	m_fSpotActive = !m_fSpotActive;

#if !CLIENT_DLL
	if( !m_fSpotActive && m_pSpot )
	{
		m_pSpot->Killed( NULL, GIB_NORMAL );
		m_pSpot = NULL;
	}
#endif
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.2f;
}

void CMl::WeaponIdle( void )
{
	UpdateSpot();

	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		ResetEmptySound();

		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0f, 1.0f );
		if( flRand <= 0.75f || m_fSpotActive )
		{
			if( m_iClip == 0 )
				iAnim = ML_IDLE_UL;
			else
				iAnim = ML_IDLE;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0f / 15.0f;
		}
		else
		{
			if( m_iClip == 0 )
				iAnim = ML_FIDGET_UL;
			else
				iAnim = ML_FIDGET;
#if WEAPONS_ANIMATION_TIMES_FIX
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.1f;
#else
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0f;
#endif
		}

		SendWeaponAnim( iAnim );
	}
	else
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
	}
}

void CMl::UpdateSpot( void )
{
#if !CLIENT_DLL
	if( m_fSpotActive )
	{
		if (m_pPlayer->pev->viewmodel == 0)
			return;

		if( !m_pSpot )
		{
			m_pSpot = CLaserSpot::CreateSpot();
		}

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition();
		Vector vecAiming = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 8192.0f, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

		UTIL_SetOrigin( m_pSpot, tr.vecEndPos );
	}
#endif
}

class CMlAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache();
		SET_MODEL( ENT( pev ), "models/w_rpgammo.mdl" );
		CBasePlayerAmmo::Spawn();
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/w_rpgammo.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int iGive;
#if CLIENT_DLL
	if( bIsMultiplayer() )
#else
	if( g_pGameRules->IsMultiplayer() )
#endif
		{
			// hand out more ammo per rocket in multiplayer.
			iGive = AMMO_RPGCLIP_GIVE * 2;
		}
		else
		{
			iGive = AMMO_RPGCLIP_GIVE;
		}

		if( pOther->GiveAmmo( iGive, "rockets", ROCKET_MAX_CARRY ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_mlclip, CMlAmmo )
#endif
