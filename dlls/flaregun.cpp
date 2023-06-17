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
#include "gamerules.h"

#if !CLIENT_DLL
#define FLARE_AIR_VELOCITY	2000
#define FLARE_WATER_VELOCITY	1000

extern BOOL g_fIsXash3D;

// UNDONE: Save/restore this?  Don't forget to set classname and LINK_ENTITY_TO_CLASS()
// 
// OVERLOADS SOME ENTVARS:
//
// speed - the ideal magnitude of my velocity
class CFlareShot : public CBaseEntity
{
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT BubbleThink( void );
	void EXPORT FlareTouch( CBaseEntity *pOther );
	void EXPORT ExplodeThink( void );

	int m_iTrail;

public:
	static CFlareShot *FlareCreate( void );
};

LINK_ENTITY_TO_CLASS( flare_shot, CFlareShot )

CFlareShot *CFlareShot::FlareCreate( void )
{
	// Create a new entity with CFlareShot private data
	CFlareShot *pFlare = GetClassPtr( (CFlareShot *)NULL );
	pFlare->pev->classname = MAKE_STRING( "flare_shot" );	// g-cont. enable save\restore
	pFlare->Spawn();

	return pFlare;
}

void CFlareShot::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	pev->gravity = 1.5f;

	SET_MODEL( ENT( pev ), "models/w_flare.mdl" );

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( &CFlareShot::FlareTouch );
	SetThink( &CFlareShot::BubbleThink );
	
	// Flare trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 64 );   // r, g, b
		WRITE_BYTE( 0 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
	
	SetNextThink( 0.2f );
}

void CFlareShot::Precache()
{
	PRECACHE_MODEL( "models/w_flare.mdl" );
	PRECACHE_SOUND( "weapons/xbow_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/xbow_hitbod2.wav" );
	PRECACHE_SOUND( "weapons/xbow_fly1.wav" );
	PRECACHE_SOUND( "weapons/xbow_hit1.wav" );
	PRECACHE_SOUND( "fvox/beep.wav" );
	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );
}

int CFlareShot::Classify( void )
{
	return CLASS_NONE;
}

void CFlareShot::FlareTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	SetThink( NULL );
	
	m_iTrail = 0;

	if( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		entvars_t *pevOwner;

		pevOwner = VARS( pev->owner );

		// UNDONE: this needs to call TraceAttack instead
		ClearMultiDamage();

		if( pOther->IsPlayer() )
		{
			pOther->TraceAttack( pevOwner, gSkillData.plrDmgFlaregunClient, pev->velocity.Normalize(), &tr, DMG_ALWAYSGIB ); 
		}
		else
		{
			pOther->TraceAttack( pevOwner, gSkillData.plrDmgFlaregunMonster, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_ALWAYSGIB ); 
		}

		ApplyMultiDamage( pev, pevOwner );

		pev->velocity = Vector( 0, 0, 0 );
		// play body "thwack" sound
		switch( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM );
			break;
		case 1:
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM );
			break;
		}

		Killed( pev, GIB_ALWAYS );
	}
	else
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT( 0.95f, 1.0f ), ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 7 ) );

		SetThink(&CFlareShot:: SUB_Remove );
		SetNextThink( 0 );// this will get changed below if the bolt is allowed to stick in what it hit.

		if( FClassnameIs( pOther->pev, "worldspawn" ) )
		{
			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = pev->velocity.Normalize();
			UTIL_SetOrigin( this, pev->origin - vecDir * 2.0f );
			pev->angles = UTIL_VecToAngles( vecDir );
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_FLY;
			pev->velocity = Vector( 0, 0, 0 );
			pev->avelocity.z = 0;
			pev->angles.z = RANDOM_LONG( 0, 360 );
			SetNextThink( 10.0f );
		}
		else if( pOther->pev->movetype == MOVETYPE_PUSH || pOther->pev->movetype == MOVETYPE_PUSHSTEP )
		{
			Vector vecDir = pev->velocity.Normalize();
			UTIL_SetOrigin( this, pev->origin - vecDir * 2.0f );
			pev->angles = UTIL_VecToAngles( vecDir );
			pev->solid = SOLID_NOT;
			pev->velocity = Vector( 0, 0, 0 );
			pev->avelocity.z = 0;
			pev->angles.z = RANDOM_LONG( 0, 360 );
			SetNextThink( 10.0f );

			if( g_fIsXash3D )
			{
				// g-cont. Setup movewith feature
				pev->movetype = MOVETYPE_COMPOUND;	// set movewith type
				pev->aiment = ENT( pOther->pev );	// set parent
			}
		}

		if( UTIL_PointContents( pev->origin ) != CONTENTS_WATER )
		{
			UTIL_Sparks( pev->origin );
		}
	}

	if( g_pGameRules->IsMultiplayer() )
	{
		SetThink( &CFlareShot::ExplodeThink );
		SetNextThink( 0.1f );
	}
}

void CFlareShot::BubbleThink( void )
{
	SetNextThink( 0.1f );

	if (pev->waterlevel == 0 || pev->watertype <= CONTENT_FLYFIELD)
		return;
	
	UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1f, pev->origin, 1 );	
}

void CFlareShot::ExplodeThink( void )
{
	int iContents = UTIL_PointContents( pev->origin );
	int iScale;

	pev->dmg = 40;
	iScale = 10;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		if( iContents != CONTENTS_WATER )
		{
			WRITE_SHORT( g_sModelIndexFireball );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		WRITE_BYTE( iScale ); // scale * 10
		WRITE_BYTE( 15 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	entvars_t *pevOwner;

	if( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, 128, CLASS_NONE, DMG_BLAST | DMG_ALWAYSGIB );

	UTIL_Remove( this );
}
#endif

enum flaregun_e
{
	FLAREGUN_IDLE1 = 0,
	FLAREGUN_IDLE2,
	FLAREGUN_FIDGET,
	FLAREGUN_SHOOT,
	FLAREGUN_DRYFIRE,
	FLAREGUN_RELOAD,
	FLAREGUN_DRAW,
	FLAREGUN_HOLSTER
};

LINK_ENTITY_TO_CLASS( weapon_flaregun, CFlaregun )

void CFlaregun::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_flaregun" ); // hack to allow for old names
	Precache();
	m_iId = WEAPON_FLAREGUN;
	SET_MODEL( ENT( pev ), "models/w_flaregun.mdl" );

	m_iDefaultAmmo = FLAREGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CFlaregun::Precache( void )
{
	PRECACHE_MODEL( "models/v_flaregun.mdl" );
	PRECACHE_MODEL( "models/w_flaregun.mdl" );
	PRECACHE_MODEL( "models/p_357.mdl" );

	m_iShell = PRECACHE_MODEL( "models/w_flare.mdl" );// brass shell

	PRECACHE_SOUND( "items/9mmclip1.wav" );
	PRECACHE_SOUND( "items/9mmclip2.wav" );

	PRECACHE_SOUND( "weapons/flaregun1.wav" );
	PRECACHE_SOUND( "weapons/flaregunreload.wav" );

	m_usFireFlaregun = PRECACHE_EVENT( 1, "events/flaregun.sc" );
}

int CFlaregun::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "Flare";
	p->iMaxAmmo1 = FLAREGUN_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = FLAREGUN_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FLAREGUN;
	p->iWeight = FLAREGUN_WEIGHT;

	return 1;
}

int CFlaregun::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CFlaregun::Deploy()
{
	// pev->body = 1;
	return DefaultDeploy( "models/v_flaregun.mdl", "models/p_357.mdl", FLAREGUN_DRAW, "python", /*UseDecrement() ? 1 : 0*/ 0 );
}

void CFlaregun::Holster( int skiplocal )
{
	m_fInReload = FALSE;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10.0f, 15.0f );
	SendWeaponAnim( FLAREGUN_HOLSTER );
}

void CFlaregun::PrimaryAttack( void )
{
	if( m_iClip <= 0 )
	{
		if( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.2f );
		}

		return;
	}

	m_iClip--;

	m_pPlayer->pev->effects = (int)( m_pPlayer->pev->effects ) | EF_MUZZLEFLASH;

	int flags;
#if CLIENT_WEAPONS
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	
	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	anglesAim.x	= -anglesAim.x;

#if !CLIENT_DLL
	Vector vecSrc	= m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 16.0f + gpGlobals->v_right * 8.0f + gpGlobals->v_up * -12.0f;
	Vector vecDir	= gpGlobals->v_forward;

	CFlareShot *pFlare = CFlareShot::FlareCreate();
	pFlare->pev->origin = vecSrc;
	pFlare->pev->angles = anglesAim;
	pFlare->pev->owner = m_pPlayer->edict();

	if (m_pPlayer->pev->waterlevel == 3 && m_pPlayer->pev->watertype > CONTENT_FLYFIELD)
	{
		pFlare->pev->velocity = vecDir * FLARE_WATER_VELOCITY;
		pFlare->pev->speed = FLARE_WATER_VELOCITY;
	}
	else
	{
		pFlare->pev->velocity = vecDir * FLARE_AIR_VELOCITY;
		pFlare->pev->speed = FLARE_AIR_VELOCITY;
	}
	pFlare->pev->avelocity.z = 10.0f;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireFlaregun, 0.0f, g_vecZero, g_vecZero, 0, 0, m_iClip, 0, 0, 0 );

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.25 );

	if( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CFlaregun::Reload( void )
{
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == FLAREGUN_MAX_CLIP )
		return;

	int iResult;

	iResult = DefaultReload( FLAREGUN_MAX_CLIP, FLAREGUN_RELOAD, 1.5f );

	if( iResult )
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
}

void CFlaregun::WeaponIdle( void )
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;
	
	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0f, 1.0f );
	if( flRand <= 0.5f )
	{
		iAnim = FLAREGUN_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 100.0f / 30.0f;
	}
	else if( flRand <= 0.7f )
	{
		iAnim = FLAREGUN_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 100.0f / 30.0f;
	}
	else
	{
		iAnim = FLAREGUN_FIDGET;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 100.0f / 30.0f;
	}
	SendWeaponAnim( iAnim, 1 );
}

class CFlaregunAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache();
		SET_MODEL( ENT( pev ), "models/w_flareround.mdl" );
		CBasePlayerAmmo::Spawn();
	}

	void Precache( void )
	{
		PRECACHE_MODEL( "models/w_flareround.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}

	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if( pOther->GiveAmmo( AMMO_FLARE_GIVE, "Flare", FLAREGUN_MAX_CARRY ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_flare, CFlaregunAmmo )

class CFlaregunBoxAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache();
		SET_MODEL( ENT( pev ), "models/w_flarebox.mdl" );
		CBasePlayerAmmo::Spawn();
	}

	void Precache( void )
	{
		PRECACHE_MODEL( "models/w_flarebox.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}

	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if( pOther->GiveAmmo( AMMO_FLAREBOX_GIVE, "Flare", FLAREGUN_MAX_CARRY ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_flarebox, CFlaregunBoxAmmo )
