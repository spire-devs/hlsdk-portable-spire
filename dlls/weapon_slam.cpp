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
#include "effects.h"
#include "gamerules.h"

#define	SLAM_PRIMARY_VOLUME		450

enum slam_e
{
	SLAM_IDLE1 = 0,
	SLAM_IDLE2,
	SLAM_ARM1,
	SLAM_ARM2,
	SLAM_FIDGET,
	SLAM_HOLSTER,
	SLAM_DRAW,
	SLAM_WORLD,
	SLAM_GROUND
};

#if !CLIENT_DLL
class CSlamTripmine : public CGrenade
{
	void Spawn( void );
	void Precache( void );
	void UpdateOnRemove();

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void EXPORT WarningThink( void );
	void EXPORT PowerupThink( void );
	void EXPORT BeamBreakThink( void );
	void EXPORT DelayDeathThink( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	void MakeBeam( void );
	void KillBeam( void );

	float m_flPowerUp;
	Vector m_vecDir;
	Vector m_vecEnd;
	float m_flBeamLength;

	EHANDLE m_hOwner;
	CBeam *m_pBeam;
	Vector m_posOwner;
	Vector m_angleOwner;
	edict_t *m_pRealOwner;// tracelines don't hit PEV->OWNER, which means a player couldn't detonate his own trip mine, so we store the owner here.
};

LINK_ENTITY_TO_CLASS( monster_slam_tripmine, CSlamTripmine )

TYPEDESCRIPTION	CSlamTripmine::m_SaveData[] =
{
	DEFINE_FIELD( CSlamTripmine, m_flPowerUp, FIELD_TIME ),
	DEFINE_FIELD( CSlamTripmine, m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( CSlamTripmine, m_vecEnd, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CSlamTripmine, m_flBeamLength, FIELD_FLOAT ),
	DEFINE_FIELD( CSlamTripmine, m_hOwner, FIELD_EHANDLE ),
#if !TRIPMINE_BEAM_DUPLICATION_FIX
	DEFINE_FIELD( CSlamTripmine, m_pBeam, FIELD_CLASSPTR ),
#endif
	DEFINE_FIELD( CSlamTripmine, m_posOwner, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CSlamTripmine, m_angleOwner, FIELD_VECTOR ),
	DEFINE_FIELD( CSlamTripmine, m_pRealOwner, FIELD_EDICT ),
};

IMPLEMENT_SAVERESTORE( CSlamTripmine, CGrenade )

void CSlamTripmine::Spawn( void )
{
	Precache();

	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;

	SET_MODEL( ENT( pev ), "models/v_tripmine.mdl" );
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = SLAM_WORLD;
	ResetSequenceInfo();
	pev->framerate = 0;
	
	UTIL_SetSize( pev, Vector( -8.0f, -8.0f, -8.0f ), Vector( 8.0f, 8.0f, 8.0f ) );
	UTIL_SetOrigin( this, pev->origin );

	if( pev->spawnflags & 1 )
	{
		// power up quickly
		m_flPowerUp = gpGlobals->time + 1.0f;
	}
	else
	{
		// power up in 2.5 seconds
		m_flPowerUp = gpGlobals->time + 2.5f;
	}

	SetThink( &CSlamTripmine::PowerupThink );
	SetNextThink( 0.2f );

	pev->takedamage = DAMAGE_YES;
	pev->dmg = gSkillData.plrDmgTripmine;
	pev->health = 1; // don't let die normally

	if( pev->owner != NULL )
	{
		// play deploy sound
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/mine_deploy.wav", 1.0, ATTN_NORM );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/slam/mine_charge.wav", 0.2, ATTN_NORM ); // chargeup

		m_pRealOwner = pev->owner;// see CSlamTripmine for why.
	}

	UTIL_MakeAimVectors( pev->angles );

	m_vecDir = gpGlobals->v_forward;
	m_vecEnd = pev->origin + m_vecDir * 2048.0f;
}

void CSlamTripmine::Precache( void )
{
	PRECACHE_MODEL( "models/v_tripmine.mdl" );
	PRECACHE_SOUND( "weapons/slam/mine_deploy.wav" );
	PRECACHE_SOUND( "weapons/slam/mine_activate.wav" );
	PRECACHE_SOUND( "weapons/slam/mine_charge.wav" );
}

void CSlamTripmine::UpdateOnRemove()
{
	CBaseEntity::UpdateOnRemove();

	KillBeam();
}

void CSlamTripmine::WarningThink( void )
{
	// play warning sound
	// EMIT_SOUND( ENT( pev ), CHAN_VOICE, "buttons/Blip2.wav", 1.0, ATTN_NORM );

	// set to power up
	SetThink( &CSlamTripmine::PowerupThink );
	SetNextThink( 1.0f );
}

void CSlamTripmine::PowerupThink( void )
{
	TraceResult tr;

	if( m_hOwner == 0 )
	{
		// find an owner
		edict_t *oldowner = pev->owner;
		pev->owner = NULL;
		UTIL_TraceLine( pev->origin + m_vecDir * 8.0f, pev->origin - m_vecDir * 32.0f, dont_ignore_monsters, ENT( pev ), &tr );
		if( tr.fStartSolid || ( oldowner && tr.pHit == oldowner ) )
		{
			pev->owner = oldowner;
			m_flPowerUp += 0.1f;
			SetNextThink( 0.1f );
			return;
		}
		if( tr.flFraction < 1.0f )
		{
			pev->owner = tr.pHit;
			m_hOwner = CBaseEntity::Instance( pev->owner );
			m_posOwner = m_hOwner->pev->origin;
			m_angleOwner = m_hOwner->pev->angles;
		}
		else
		{
			STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/mine_deploy.wav" );
			STOP_SOUND( ENT( pev ), CHAN_BODY, "weapons/slam/mine_charge.wav" );
			SetThink( &CSlamTripmine::SUB_Remove );
			SetNextThink( 0.1f );
			ALERT( at_console, "WARNING:Tripmine at %.0f, %.0f, %.0f removed\n", (double)pev->origin.x, (double)pev->origin.y, (double)pev->origin.z );
			KillBeam();
			return;
		}
	}
	else if( m_posOwner != m_hOwner->pev->origin || m_angleOwner != m_hOwner->pev->angles )
	{
		// disable
		STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/mine_deploy.wav" );
		STOP_SOUND( ENT( pev ), CHAN_BODY, "weapons/slam/mine_charge.wav" );
		CBaseEntity *pMine = Create( "weapon_slam_tripmine", pev->origin + m_vecDir * 24.0f, pev->angles );
		pMine->pev->spawnflags |= SF_NORESPAWN;

		SetThink(&CSlamTripmine :: SUB_Remove );
		KillBeam();
		SetNextThink( 0.1f );
		return;
	}
	// ALERT( at_console, "%d %.0f %.0f %0.f\n", pev->owner, m_pOwner->pev->origin.x, m_pOwner->pev->origin.y, m_pOwner->pev->origin.z );
 
	if( gpGlobals->time > m_flPowerUp )
	{
		// make solid
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin( this, pev->origin );

		MakeBeam();

		// play enabled sound
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "weapons/slam/mine_activate.wav", 0.5, ATTN_NORM, 1, 75 );
	}
	SetNextThink( 0.1f );
}

void CSlamTripmine::KillBeam( void )
{
	if( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}

void CSlamTripmine::MakeBeam( void )
{
	TraceResult tr;

	// ALERT( at_console, "serverflags %f\n", gpGlobals->serverflags );

	UTIL_TraceLine( pev->origin, m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	m_flBeamLength = tr.flFraction;

	// set to follow laser spot
	SetThink( &CSlamTripmine::BeamBreakThink );
	SetNextThink( 0.1f );

	Vector vecTmpEnd = pev->origin + m_vecDir * 2048.0f * m_flBeamLength;

	m_pBeam = CBeam::BeamCreate( g_pModelNameLaser, 10 );
#if TRIPMINE_BEAM_DUPLICATION_FIX
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
#endif
	m_pBeam->PointEntInit( vecTmpEnd, entindex() );
	m_pBeam->SetColor( 0, 214, 198 );
	m_pBeam->SetScrollRate( 255 );
	m_pBeam->SetBrightness( 64 );
}

void CSlamTripmine::BeamBreakThink( void )
{
	BOOL bBlowup = 0;

	TraceResult tr;

	// HACKHACK Set simple box using this really nice global!
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine( pev->origin, m_vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	// ALERT( at_console, "%f : %f\n", tr.flFraction, m_flBeamLength );

	// respawn detect. 
	if( !m_pBeam )
	{
#if TRIPMINE_BEAM_DUPLICATION_FIX
		// Use the same trace parameters as the original trace above so the right entity is hit.
		TraceResult tr2;
		UTIL_TraceLine( pev->origin + m_vecDir * 8.0f, pev->origin - m_vecDir * 32.0f, dont_ignore_monsters, ENT( pev ), &tr2 );
#endif
		MakeBeam();
#if TRIPMINE_BEAM_DUPLICATION_FIX
		if( tr2.pHit )
		{
			// reset owner too
			pev->owner = tr2.pHit;
			m_hOwner = CBaseEntity::Instance( tr2.pHit );
		}
#else
		if( tr.pHit )
			m_hOwner = CBaseEntity::Instance( tr.pHit );	// reset owner too
#endif
	}

	if( fabs( m_flBeamLength - tr.flFraction ) > 0.001f )
	{
		bBlowup = 1;
	}
	else
	{
		if( m_hOwner == 0 )
			bBlowup = 1;
		else if( m_posOwner != m_hOwner->pev->origin )
			bBlowup = 1;
		else if( m_angleOwner != m_hOwner->pev->angles )
			bBlowup = 1;
	}

	if( bBlowup )
	{
		// a bit of a hack, but all CGrenade code passes pev->owner along to make sure the proper player gets credit for the kill
		// so we have to restore pev->owner from pRealOwner, because an entity's tracelines don't strike it's pev->owner which meant
		// that a player couldn't trigger his own tripmine. Now that the mine is exploding, it's safe the restore the owner so the 
		// CGrenade code knows who the explosive really belongs to.
		pev->owner = m_pRealOwner;
		pev->health = 0;
		Killed( VARS( pev->owner ), GIB_NORMAL );
		return;
	}

	SetNextThink( 0.1f );
}

int CSlamTripmine::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( gpGlobals->time < m_flPowerUp && flDamage < pev->health )
	{
		// disable
		// Create( "weapon_tripmine", pev->origin + m_vecDir * 24, pev->angles );
		SetThink(&CSlamTripmine :: SUB_Remove );
		SetNextThink( 0.1f );
		KillBeam();
		return FALSE;
	}
	return CGrenade::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CSlamTripmine::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;

	if( pevAttacker && ( pevAttacker->flags & FL_CLIENT ) )
	{
		// some client has destroyed this mine, he'll get credit for any kills
		pev->owner = ENT( pevAttacker );
	}

	SetThink( &CSlamTripmine::DelayDeathThink );
	SetNextThink( RANDOM_FLOAT( 0.1f, 0.3f ) );

	EMIT_SOUND( ENT( pev ), CHAN_BODY, "common/null.wav", 0.5f, ATTN_NORM ); // shut off chargeup
}

void CSlamTripmine::DelayDeathThink( void )
{
	KillBeam();
	TraceResult tr;
	UTIL_TraceLine( pev->origin + m_vecDir * 8, pev->origin - m_vecDir * 64.0f,  dont_ignore_monsters, ENT( pev ), &tr );

	Explode( &tr, DMG_BLAST );
}
#endif

class CSlamSatchel : public CGrenade
{
	Vector m_lastBounceOrigin;	// Used to fix a bug in engine: when object isn't moving, but its speed isn't 0 and on ground isn't set
	void Spawn( void );
	void Precache( void );
	void BounceSound( void );

	void EXPORT SatchelSlide( CBaseEntity *pOther );
	void EXPORT SatchelThink( void );

public:
	void Deactivate( void );
	Vector m_slamvelocity;
};

LINK_ENTITY_TO_CLASS( monster_slam_satchel, CSlamSatchel )

//=========================================================
// Deactivate - do whatever it is we do to an orphaned 
// satchel when we don't want it in the world anymore.
//=========================================================
void CSlamSatchel::Deactivate( void )
{
	pev->solid = SOLID_NOT;
	UTIL_Remove( this );
}

void CSlamSatchel::Spawn( void )
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/w_satchel.mdl" );
	//UTIL_SetSize( pev, Vector( -16, -16, -4 ), Vector( 16, 16, 32 ) );	// Old box -- size of headcrab monsters/players get blocked by this
	UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );	// Uses point-sized, and can be stepped over
	UTIL_SetOrigin( this, pev->origin );

	SetTouch( &CSlamSatchel::SatchelSlide );
	SetUse(&CSlamSatchel :: DetonateUse );
	SetThink( &CSlamSatchel::SatchelThink );
	SetNextThink( 0.1f );

	pev->gravity = 0.5f;
	pev->friction = 0.8f;
	pev->velocity = m_slamvelocity + gpGlobals->v_forward * 274;
	pev->avelocity.y = 400;

	pev->dmg = gSkillData.plrDmgSatchel;
	// ResetSequenceInfo();
	pev->sequence = 1;
}

void CSlamSatchel::SatchelSlide( CBaseEntity *pOther )
{
	//entvars_t *pevOther = pOther->pev;

	// don't hit the guy that launched this grenade
	if( pOther->edict() == pev->owner )
		return;

	// pev->avelocity = Vector( 300, 300, 300 );
	pev->gravity = 1;// normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 10 ), ignore_monsters, edict(), &tr );

	if( tr.flFraction < 1.0f )
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;
		pev->avelocity = pev->avelocity * 0.9;
		// play sliding sound, volume based on velocity
	}
	if( !( pev->flags & FL_ONGROUND ) && pev->velocity.Length2D() > 10.0f )
	{
		// Fix for a bug in engine: when object isn't moving, but its speed isn't 0 and on ground isn't set
		if( pev->origin != m_lastBounceOrigin )
		BounceSound();
	}
	m_lastBounceOrigin = pev->origin;
	// There is no model animation so commented this out to prevent net traffic
	// StudioFrameAdvance();
}

void CSlamSatchel::SatchelThink( void )
{
	//StudioFrameAdvance( );
	SetNextThink( 0.1f );

	if( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}

	if (pev->waterlevel == 3 && pev->watertype != CONTENT_FOG)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 0.8f;
		pev->avelocity = pev->avelocity * 0.9f;
		pev->velocity.z += 8;
	}
	else if (pev->waterlevel == 0 || pev->watertype == CONTENT_FOG)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		pev->velocity.z -= 8.0f;
	}	
}

void CSlamSatchel::Precache( void )
{
	PRECACHE_MODEL( "models/w_satchel.mdl" );
	PRECACHE_SOUND( "weapons/slam/slam_bounce_1.wav" );
	PRECACHE_SOUND( "weapons/slam/slam_bounce_2.wav" );
	PRECACHE_SOUND( "weapons/slam/slam_bounce_3.wav" );
}

void CSlamSatchel::BounceSound( void )
{
	switch( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/slam_bounce_1.wav", 1, ATTN_NORM );
		break;
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/slam_bounce_2.wav", 1, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/slam_bounce_3.wav", 1, ATTN_NORM );
		break;
	}
}

LINK_ENTITY_TO_CLASS( weapon_slam, CSlam )

void CSlam::Spawn()
{
	Precache();
	m_iId = WEAPON_SLAM;
	SET_MODEL( ENT( pev ), "models/v_tripmine.mdl" );
	pev->frame = 0;

#ifdef CLIENT_DLL
	pev->body = 0;
#else
	pev->body = 3;
#endif
	pev->sequence = SLAM_GROUND;
	// ResetSequenceInfo();
	pev->framerate = 0;

	FallInit();// get ready to fall down

	m_iDefaultAmmo = SLAM_DEFAULT_GIVE;

#if CLIENT_DLL
	if( !bIsMultiplayer() )
#else
	if( !g_pGameRules->IsDeathmatch() )
#endif
	{
		UTIL_SetSize( pev, Vector( -16.0f, -16.0f, 0.0f ), Vector( 16.0f, 16.0f, 28.0f ) ); 
	}
}

void CSlam::Precache( void )
{
	PRECACHE_MODEL( "models/v_tripmine.mdl" );
	PRECACHE_MODEL( "models/p_tripmine.mdl" );
	PRECACHE_SOUND( "weapons/slam/slam_detonator.wav" );
	PRECACHE_SOUND( "weapons/slam/slam_throw.wav" );
	PRECACHE_SOUND( "weapons/slam/slam_throw_mode.wav" );
	UTIL_PrecacheOther( "monster_slam_tripmine" );
	UTIL_PrecacheOther( "monster_slam_satchel" );

	m_usSlamFire = PRECACHE_EVENT( 1, "events/slam.sc" );
}

int CSlam::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "SLAM";
	p->iMaxAmmo1 = SLAM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_SLAM;
	p->iWeight = SLAM_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CSlam::Deploy()
{
	pev->body = 0;
	return DefaultDeploy( "models/v_tripmine.mdl", "models/p_tripmine.mdl", SLAM_DRAW, "trip" );
}

void CSlam::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		// out of mines
		m_pPlayer->pev->weapons &= ~( 1 << WEAPON_SLAM );
		SetThink(&CSlam:: DestroyItem );
		SetNextThink( 0.1 );
	}

	SendWeaponAnim( SLAM_HOLSTER );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "common/null.wav", 1.0f, ATTN_NORM );
}

BOOL CSlam::CanAttach()
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 128.0f, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
	
	if( tr.flFraction < 1.0f )
	{
		return true;
	}
	
	return false;
}

void CSlam::PrimaryAttack( void )
{
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;
	
	if( CanAttach() )
	{
		Attach();
	}
	else
	{
		Throw();
	}

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.3 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.3 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CSlam::SecondaryAttack( void )
{

	edict_t *pPlayer = m_pPlayer->edict();

	CBaseEntity *pSatchel = NULL;

	while( ( pSatchel = UTIL_FindEntityInSphere( pSatchel, m_pPlayer->pev->origin, 4096 ) ) != NULL )
	{
		if( FClassnameIs( pSatchel->pev, "monster_slam_satchel" ) )
		{
			if( pSatchel->pev->owner == pPlayer )
			{
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/slam_detonator.wav", 1, ATTN_NORM );
				pSatchel->Use( m_pPlayer, m_pPlayer, USE_ON, 0 );
			}
		}
	}

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.1 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CSlam::Attach( void )
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 128.0f, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	int flags;
#if CLIENT_WEAPONS
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSlamFire, 0.0f, g_vecZero, g_vecZero, 0.0f, 0.0f, 0, 0, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 1, 0 );
	
	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
	if( pEntity && !( pEntity->pev->flags & FL_CONVEYOR ) )
	{
		Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );

		CBaseEntity::Create( "monster_slam_tripmine", tr.vecEndPos + tr.vecPlaneNormal * 8.0f, angles, m_pPlayer->edict() );

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
		if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		{
			// no more mines! 
			RetireWeapon();
			return;
		}
	}
}

void CSlam::Throw( void )
{
	CBaseEntity *pSatchel;
	
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/slam/slam_throw.wav", 1, ATTN_NORM );
		
		CBaseEntity *pSatchel = Create( "monster_slam_satchel", m_pPlayer->GetGunPosition(), Vector( 0, 0, 0 ), m_pPlayer->edict() );
		
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}
}

void CSlam::WeaponIdle( void )
{
	pev->body = 0;

	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 )
	{
		SendWeaponAnim( SLAM_DRAW );
	}
	else
	{
		RetireWeapon(); 
		return;
	}

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if( flRand <= 0.25f )
	{
		iAnim = SLAM_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0f / 30.0f;
	}
	else if( flRand <= 0.75f )
	{
		iAnim = SLAM_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0f / 30.0f;
	}
	else
	{
		iAnim = SLAM_FIDGET;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 100.0f / 30.0f;
	}

	SendWeaponAnim( iAnim );
}
