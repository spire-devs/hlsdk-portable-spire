/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// hgrunt
//=========================================================

//=========================================================
// Hit groups!	
//=========================================================
/*

  1 - Head
  2 - Stomach
  3 - Gun

*/

#include	"extdll.h"
#include	"plane.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"squadmonster.h"
#include	"weapons.h"
#include	"talkmonster.h"
#include	"soundent.h"
#include	"effects.h"
#include	"customentity.h"
#include	"scripted.h" //LRC

int g_fWPoliceQuestion;				// true if an idle grunt asked a question. Cleared when someone answers.

extern DLL_GLOBAL int		g_iSkillLevel;

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	WPOLICE_CLIP_SIZE					30 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define WPOLICE_VOL						0.35		// volume of grunt sounds
#define WPOLICE_ATTN						ATTN_NORM	// attenutation of grunt sentences
#define WPOLICE_LIMP_HEALTH				20
#define WPOLICE_MINIMUM_HEADSHOT_DAMAGE	15 // must do at least this much damage in one shot to head to score a headshot kill
#define	WPOLICE_SENTENCE_VOLUME			(float)0.35 // volume of grunt sentences

#define WPOLICE_SMG1				( 1 << 0 ) // 1
#define WPOLICE_PISTOL				( 1 << 1 ) // 2
#define WPOLICE_FRAG				( 1 << 2 ) // 4
#define WPOLICE_GRENADELAUNCHER		( 1 << 3 ) // 8 : OBSOLETE

//////////////////////////////
// 01 : SMG1 		- YES	//
// 02 : PISTOL		- YES	//
// 04 : HG			- NO	//
// 05 : SMG1 + HG	- NO	//
// 06 : PISTOL + HG	- NO	//
// 08 : GL			- NO	//
//////////////////////////////

#define GUN_GROUP					1
#define GUN_SMG1					0
#define GUN_PISTOL					1
#define GUN_NONE					3

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		WPOLICE_AE_RELOAD		( 2 )
#define		WPOLICE_AE_KICK			( 3 )
#define		WPOLICE_AE_BURST1		( 4 )
#define		WPOLICE_AE_BURST2		( 5 ) 
#define		WPOLICE_AE_BURST3		( 6 ) 
#define		WPOLICE_AE_GREN_TOSS		( 7 )
#define		WPOLICE_AE_GREN_LAUNCH	( 8 )
#define		WPOLICE_AE_GREN_DROP		( 9 )
#define		WPOLICE_AE_CAUGHT_ENEMY	( 10 ) // grunt established sight with an enemy (player only) that had previously eluded the squad.
#define		WPOLICE_AE_DROP_GUN		( 11 ) // grunt (probably dead) is dropping his mp5.

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_WPOLICE_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_WPOLICE_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_WPOLICE_COVER_AND_RELOAD,
	SCHED_WPOLICE_SWEEP,
	SCHED_WPOLICE_FOUND_ENEMY,
	SCHED_WPOLICE_REPEL,
	SCHED_WPOLICE_REPEL_ATTACK,
	SCHED_WPOLICE_REPEL_LAND,
	SCHED_WPOLICE_WAIT_FACE_ENEMY,
	SCHED_WPOLICE_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_WPOLICE_ELOF_FAIL
};

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_WPOLICE_FACE_TOSS_DIR = LAST_COMMON_TASK + 1,
	TASK_WPOLICE_SPEAK_SENTENCE,
	TASK_WPOLICE_CHECK_FIRE
};

//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_WPOLICE_NOFIRE	( bits_COND_SPECIAL1 )

class CWPolice : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int Classify( void );
	int ISoundMask( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL FCanCheckAttacks( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	void CheckAmmo( void );
	void SetActivity( Activity NewActivity );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	void DeathSound( void );
	void PainSound( void );
	void IdleSound( void );
	Vector GetGunPosition( void );
	void Shoot( void );
	void SMG1( void );
	void Pistol( void );
	void PrescheduleThink( void );
	void GibMonster( void );
	void SpeakSentence( void );

	int Save( CSave &save ); 
	int Restore( CRestore &restore );

	CBaseEntity *Kick( void );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	int IRelationship( CBaseEntity *pTarget );

	BOOL FOkToSpeak( void );
	void JustSpoke( void );

	CUSTOM_SCHEDULES
	static TYPEDESCRIPTION m_SaveData[];

	// checking the feasibility of a grenade toss is kind of costly, so we do it every couple of seconds,
	// not every server frame.
	float m_flNextGrenadeCheck;
	float m_flNextPainTime;
	float m_flLastEnemySightTime;

	Vector m_vecTossVelocity;

	BOOL m_fThrowGrenade;
	BOOL m_fStanding;
	BOOL m_fFirstEncounter;// only put on the handsign show in the squad's first encounter.
	int m_cClipSize;

	int m_voicePitch;

	int m_iBrassShell;
	int m_iShotgunShell;

	int m_iSentence;

	static const char *pWPoliceSentences[];
};

LINK_ENTITY_TO_CLASS( monster_horde_police, CWPolice )

TYPEDESCRIPTION	CWPolice::m_SaveData[] =
{
	DEFINE_FIELD( CWPolice, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( CWPolice, m_flNextPainTime, FIELD_TIME ),
	//DEFINE_FIELD( CWPolice, m_flLastEnemySightTime, FIELD_TIME ), // don't save, go to zero
	DEFINE_FIELD( CWPolice, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CWPolice, m_fThrowGrenade, FIELD_BOOLEAN ),
	DEFINE_FIELD( CWPolice, m_fStanding, FIELD_BOOLEAN ),
	DEFINE_FIELD( CWPolice, m_fFirstEncounter, FIELD_BOOLEAN ),
	DEFINE_FIELD( CWPolice, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CWPolice, m_voicePitch, FIELD_INTEGER ),
	//DEFINE_FIELD( CShotgun, m_iBrassShell, FIELD_INTEGER ),
	//DEFINE_FIELD( CShotgun, m_iShotgunShell, FIELD_INTEGER ),
	DEFINE_FIELD( CWPolice, m_iSentence, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWPolice, CSquadMonster )

const char *CWPolice::pWPoliceSentences[] =
{
	"HG_GREN", // grenade scared grunt
	"HG_ALERT", // sees player
	"HG_MONSTER", // sees monster
	"HG_COVER", // running to cover
	"HG_THROW", // about to throw grenade
	"HG_CHARGE",  // running out to get the enemy
	"HG_TAUNT", // say rude things
};

typedef enum
{
	WPOLICE_SENT_NONE = -1,
	WPOLICE_SENT_GREN = 0,
	WPOLICE_SENT_ALERT,
	WPOLICE_SENT_MONSTER,
	WPOLICE_SENT_COVER,
	WPOLICE_SENT_THROW,
	WPOLICE_SENT_CHARGE,
	WPOLICE_SENT_TAUNT
} WPOLICE_SENTENCE_TYPES;

//=========================================================
// Speak Sentence - say your cued up sentence.
//
// Some grunt sentences (take cover and charge) rely on actually
// being able to execute the intended action. It's really lame
// when a grunt says 'COVER ME' and then doesn't move. The problem
// is that the sentences were played when the decision to TRY
// to move to cover was made. Now the sentence is played after 
// we know for sure that there is a valid path. The schedule
// may still fail but in most cases, well after the grunt has 
// started moving.
//=========================================================
void CWPolice::SpeakSentence( void )
{
	if( m_iSentence == WPOLICE_SENT_NONE )
	{
		// no sentence cued up.
		return; 
	}

	if( FOkToSpeak() )
	{
		SENTENCEG_PlayRndSz( ENT( pev ), pWPoliceSentences[m_iSentence], WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
		JustSpoke();
	}
}

//=========================================================
// IRelationship - overridden because Alien WPolices are 
// Human WPolice's nemesis.
//=========================================================
int CWPolice::IRelationship( CBaseEntity *pTarget )
{
	//LRC- only hate alien grunts if my behaviour hasn't been overridden
	if( ( !m_iClass && FClassnameIs( pTarget->pev, "monster_alien_grunt" ) ) || ( FClassnameIs( pTarget->pev,  "monster_gargantua" ) ) )
	{
		return R_NM;
	}

	return CSquadMonster::IRelationship( pTarget );
}

//=========================================================
// GibMonster - make gun fly through the air.
//=========================================================
void CWPolice::GibMonster( void )
{
	Vector vecGunPos;
	Vector vecGunAngles;

	if( GetBodygroup( GUN_GROUP ) != GUN_NONE )
	{
		// throw a gun if the grunt has one
		GetAttachment( 0, vecGunPos, vecGunAngles );

		CBaseEntity *pGun;

		if( FBitSet( pev->weapons, WPOLICE_PISTOL ) )
		{
			pGun = DropItem( "weapon_pistol", vecGunPos, vecGunAngles );
		}
		else
		{
			pGun = DropItem( "weapon_smg1", vecGunPos, vecGunAngles );
		}

		if( pGun )
		{
			pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
			pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
		}

		if( FBitSet( pev->weapons, WPOLICE_GRENADELAUNCHER ) )
		{
			pGun = DropItem( "ammo_ARgrenades", vecGunPos, vecGunAngles );
			if ( pGun )
			{
				pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
				pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
			}
		}
	}

	CBaseMonster::GibMonster();
}

//=========================================================
// ISoundMask - Overidden for human grunts because they 
// hear the DANGER sound that is made by hand grenades and
// other dangerous items.
//=========================================================
int CWPolice::ISoundMask( void )
{
	return	bits_SOUND_WORLD |
			bits_SOUND_COMBAT |
			bits_SOUND_PLAYER |
			bits_SOUND_DANGER;
}

//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CWPolice::FOkToSpeak( void )
{
	// if someone else is talking, don't speak
	if( gpGlobals->time <= CTalkMonster::g_talkWaitTime )
		return FALSE;

	if( pev->spawnflags & SF_MONSTER_GAG )
	{
		if( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			// no talking outside of combat if gagged.
			return FALSE;
		}
	}

	// if player is not in pvs, don't speak
	//if( FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
	//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
void CWPolice::JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5f, 2.0f );
	m_iSentence = WPOLICE_SENT_NONE;
}

//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CWPolice::PrescheduleThink( void )
{
	if( InSquad() && m_hEnemy != 0 )
	{
		if( HasConditions( bits_COND_SEE_ENEMY ) )
		{
			// update the squad's last enemy sighting time.
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
		}
		else
		{
			if( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5.0f )
			{
				// been a while since we've seen the enemy
				MySquadLeader()->m_fEnemyEluded = TRUE;
			}
		}
	}
}

//=========================================================
// FCanCheckAttacks - this is overridden for human grunts
// because they can throw/shoot grenades when they can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
BOOL CWPolice::FCanCheckAttacks( void )
{
	if( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CWPolice::CheckMeleeAttack1( float flDot, float flDist )
{
	CBaseMonster *pEnemy = 0;

	if( m_hEnemy != 0 )
	{
		pEnemy = m_hEnemy->MyMonsterPointer();

		if( !pEnemy )
		{
			return FALSE;
		}

		if( flDist <= 64.0f && flDot >= 0.7f && 
			 pEnemy->Classify() != CLASS_ALIEN_BIOWEAPON &&
			 pEnemy->Classify() != CLASS_WARRIOR_BIOWEAPON &&
			 pEnemy->Classify() != CLASS_PLAYER_BIOWEAPON )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - overridden for WPolice, cause 
// FCanCheckAttacks() doesn't disqualify all attacks based
// on whether or not the enemy is occluded because unlike
// the base class, the WPolice can attack when the enemy is
// occluded (throw grenade over wall, etc). We must 
// disqualify the machine gun attack if the enemy is occluded.
//=========================================================
BOOL CWPolice::CheckRangeAttack1( float flDot, float flDist )
{
	if( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048.0f && flDot >= 0.5f && NoFriendlyFire() )
	{
		TraceResult tr;

		if( !m_hEnemy->IsPlayer() && flDist <= 64 )
		{
			// kick nonclients who are close enough, but don't shoot at them.
			return FALSE;
		}

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if( tr.flFraction == 1.0f )
		{
			return TRUE;
		}
	}

	return FALSE;
}

//=========================================================
// CheckRangeAttack2 - this checks the WPolice's grenade
// attack. 
//=========================================================
BOOL CWPolice::CheckRangeAttack2( float flDot, float flDist )
{
	if( !FBitSet( pev->weapons, ( WPOLICE_FRAG | WPOLICE_GRENADELAUNCHER ) ) )
	{
		return FALSE;
	}
	
	// if the grunt isn't moving, it's ok to check.
	if( m_flGroundSpeed != 0 )
	{
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	// assume things haven't changed too much since last time
	if( gpGlobals->time < m_flNextGrenadeCheck )
	{
		return m_fThrowGrenade;
	}

	if ( !FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND ) && (m_hEnemy->pev->waterlevel == 0 || m_hEnemy->pev->watertype==CONTENT_FOG) && m_vecEnemyLKP.z > pev->absmax.z  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	Vector vecTarget;

	if( FBitSet( pev->weapons, WPOLICE_FRAG ) )
	{
		// find feet
		if( RANDOM_LONG( 0, 1 ) )
		{
			// magically know where they are
			vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
		}
		else
		{
			// toss it to where you last saw them
			vecTarget = m_vecEnemyLKP;
		}
		// vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
		// estimate position
		// vecTarget = vecTarget + m_hEnemy->pev->velocity * 2;
	}
	else
	{
		// find target
		// vecTarget = m_hEnemy->BodyTarget( pev->origin );
		vecTarget = m_vecEnemyLKP + ( m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin );
		// estimate position
		if( HasConditions( bits_COND_SEE_ENEMY ) )
			vecTarget = vecTarget + ( ( vecTarget - pev->origin).Length() / gSkillData.wgruntGrenadeSpeed ) * m_hEnemy->pev->velocity;
	}

	// are any of my squad members near the intended grenade impact area?
	if( InSquad() )
	{
		if( SquadMemberInRange( vecTarget, 256 ) )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			m_fThrowGrenade = FALSE;
		}
	}

	if( ( vecTarget - pev->origin ).Length2D() <= 256.0f )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	if( FBitSet( pev->weapons, WPOLICE_FRAG ) )
	{
		Vector vecToss = VecCheckToss( pev, GetGunPosition(), vecTarget, 0.5 );

		if( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1.0f; // one full second.
		}
	}
	else
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, gSkillData.wgruntGrenadeSpeed, 0.5 );

		if( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 0.3f; // 1/3 second.
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1.0f; // one full second.
		}
	}

	return m_fThrowGrenade;
}

//=========================================================
// TraceAttack - make sure we're not taking it in the helmet
//=========================================================
void CWPolice::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	// check for helmet shot
	if( ptr->iHitgroup == 11 )
	{
		/*
		// make sure we're wearing one
		if( GetBodygroup( HEAD_GROUP ) == HEAD_WPOLICE && ( bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB ) ) )
		{
			// absorb damage
			flDamage -= 20;
			if( flDamage <= 0 )
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01f;
			}
		}
		*/
		// it's head shot anyways
		ptr->iHitgroup = HITGROUP_HEAD;
	}
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// TakeDamage - overridden for the grunt because the grunt
// needs to forget that he is in cover if he's hurt. (Obviously
// not in a safe place anymore).
//=========================================================
int CWPolice::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Forget( bits_MEMORY_INCOVER );

	return CSquadMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CWPolice::SetYawSpeed( void )
{
	int ys;

	switch( m_Activity )
	{
	case ACT_IDLE:	
		ys = 150;
		break;
	case ACT_RUN:	
		ys = 150;	
		break;
	case ACT_WALK:	
		ys = 180;		
		break;
	case ACT_RANGE_ATTACK1:	
		ys = 120;	
		break;
	case ACT_RANGE_ATTACK2:	
		ys = 120;	
		break;
	case ACT_MELEE_ATTACK1:	
		ys = 120;	
		break;
	case ACT_MELEE_ATTACK2:	
		ys = 120;	
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:	
		ys = 180;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		ys = 30;
		break;
	default:
		ys = 90;
		break;
	}

	pev->yaw_speed = ys;
}

void CWPolice::IdleSound( void )
{
	if( FOkToSpeak() && ( g_fWPoliceQuestion || RANDOM_LONG( 0, 1 ) ) )
	{
		if( !g_fWPoliceQuestion )
		{
			// ask question or make statement
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0:
				// check in
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_CHECK", WPOLICE_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				g_fWPoliceQuestion = 1;
				break;
			case 1:
				// question
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_QUEST", WPOLICE_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				g_fWPoliceQuestion = 2;
				break;
			case 2:
				// statement
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_IDLE", WPOLICE_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				break;
			}
		}
		else
		{
			switch( g_fWPoliceQuestion )
			{
			case 1:
				// check in
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_CLEAR", WPOLICE_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				break;
			case 2:
				// question 
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_ANSWER", WPOLICE_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				break;
			}
			g_fWPoliceQuestion = 0;
		}
		JustSpoke();
	}
}

//=========================================================
// CheckAmmo - overridden for the grunt because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CWPolice::CheckAmmo( void )
{
	if( m_cAmmoLoaded <= 0 )
	{
		SetConditions( bits_COND_NO_AMMO_LOADED );
	}
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int CWPolice::Classify( void )
{
	return m_iClass?m_iClass:CLASS_WARRIOR_MILITARY;
}

//=========================================================
//=========================================================
CBaseEntity *CWPolice::Kick( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5f;
	Vector vecEnd = vecStart + ( gpGlobals->v_forward * 70 );

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );

	if( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}

	return NULL;
}

//=========================================================
// GetGunPosition	return the end of the barrel
//=========================================================

Vector CWPolice::GetGunPosition()
{
	if( m_fStanding )
	{
		return pev->origin + Vector( 0, 0, 60 );
	}
	else
	{
		return pev->origin + Vector( 0, 0, 48 );
	}
}

//=========================================================
// Shoot
//=========================================================
void CWPolice::Shoot( void )
{
	switch ( pev->weapons )
	{
		case WPOLICE_SMG1:
		{
			SMG1();
		}	
		break;
		
		case WPOLICE_PISTOL:
		{
			Pistol();
		}	
		break;
	}
}

//=========================================================
// Shoot - SMG1
//=========================================================
void CWPolice::SMG1( void )
{
	if (m_hEnemy == 0 && m_pCine == 0) //LRC - scripts may fire when you have no enemy
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	if (m_cAmmoLoaded > 0)
	{
	UTIL_MakeVectors( pev->angles );

	Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_5DEGREES, 2048, BULLET_MONSTER_SMALLROUND ); // shoot +-4 degrees

	pev->effects |= EF_MUZZLEFLASH;

	m_cAmmoLoaded--;// take away a bullet!
	}

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}

//=========================================================
// Shoot - Pistol
//=========================================================
void CWPolice::Pistol( void )
{
	if (m_hEnemy == 0 && m_pCine == 0) //LRC - scripts may fire when you have no enemy
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	if (m_cAmmoLoaded > 0)
	{
	UTIL_MakeVectors( pev->angles );

	Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_3DEGREES, 2048, BULLET_MONSTER_SMALLROUND ); // shoot +-4 degrees

	pev->effects |= EF_MUZZLEFLASH;

	m_cAmmoLoaded--;// take away a bullet!
	}

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CWPolice::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector vecShootDir;
	Vector vecShootOrigin;

	switch( pEvent->event )
	{
		case WPOLICE_AE_DROP_GUN:
		{
			if (pev->spawnflags & SF_MONSTER_NO_WPN_DROP) break; //LRC

			Vector vecGunPos;
			Vector vecGunAngles;

				GetAttachment( 0, vecGunPos, vecGunAngles );

				// switch to body group with no gun.
				SetBodygroup( GUN_GROUP, GUN_NONE );

				// now spawn a gun.
				if( FBitSet( pev->weapons, WPOLICE_PISTOL ) )
				{
					DropItem( "weapon_pistol", vecGunPos, vecGunAngles );
				}
				else
				{
					DropItem( "weapon_smg1", vecGunPos, vecGunAngles );
				}

				if( FBitSet( pev->weapons, WPOLICE_GRENADELAUNCHER ) )
				{
					DropItem( "ammo_ARgrenades", BodyTarget( pev->origin ), vecGunAngles );
				}
			
		}
			break;
		case WPOLICE_AE_RELOAD: {
			EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
		}	break;
		case WPOLICE_AE_GREN_TOSS:
		{
			UTIL_MakeVectors( pev->angles );
			// CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 34 + Vector( 0, 0, 32 ), m_vecTossVelocity, 3.5 );
			//LRC - a bit of a hack. Ideally the grunts would work out in advance whether it's ok to throw.
			if (m_pCine)
			{
				Vector vecToss = g_vecZero;
				if (m_hTargetEnt != 0 && m_pCine->PreciseAttack())
				{
					vecToss = VecCheckToss( pev, GetGunPosition(), m_hTargetEnt->pev->origin, 0.5 );
				}
				if (vecToss == g_vecZero)
				{
					vecToss = (gpGlobals->v_forward*0.5+gpGlobals->v_up*0.5).Normalize()*gSkillData.wgruntGrenadeSpeed;
				}
				CGrenade::ShootTimed( pev, GetGunPosition(), vecToss, 3.5 );
			}
			else
			CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5 );

			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
			// !!!LATER - when in a group, only try to throw grenade if ordered.
		
		}	break;
		case WPOLICE_AE_GREN_LAUNCH:
		{
			EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM );
			//LRC: firing due to a script?
			if (m_pCine)
			{
				Vector vecToss;
				if (m_hTargetEnt != 0 && m_pCine->PreciseAttack())
					vecToss = VecCheckThrow( pev, GetGunPosition(), m_hTargetEnt->pev->origin, gSkillData.wgruntGrenadeSpeed, 0.5 );
				else
				{
					// just shoot diagonally up+forwards
					UTIL_MakeVectors(pev->angles);
					vecToss = (gpGlobals->v_forward*0.5 + gpGlobals->v_up*0.5).Normalize() * gSkillData.wgruntGrenadeSpeed;
				}
				CGrenade::ShootContact( pev, GetGunPosition(), vecToss );
			}
			else
			CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
			m_fThrowGrenade = FALSE;
			if( g_iSkillLevel == SKILL_HARD )
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 2.0f, 5.0f );// wait a random amount of time before shooting again
			else
				m_flNextGrenadeCheck = gpGlobals->time + 6.0f;// wait six seconds before even looking again to see if a grenade can be thrown.
		
		}	break;
		case WPOLICE_AE_GREN_DROP:
		{
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
		
		}	break;
		case WPOLICE_AE_BURST1:
		{
			if( FBitSet( pev->weapons, WPOLICE_SMG1 ) )
			{
				// the first round of the three round burst plays the sound and puts a sound in the world sound list.
				if (m_cAmmoLoaded > 0)
				{
					Shoot();
					
					if( RANDOM_LONG( 0, 2 ) == 1 )
					{
						EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/smg1/smg1_3rounds_1.wav", 1, ATTN_NORM );
					}
					else if ( RANDOM_LONG( 0, 1 ) == 1 )
					{
						EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/smg1/smg1_3rounds_2.wav", 1, ATTN_NORM );
					}
					else
					{
						EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/smg1/smg1_3rounds_3.wav", 1, ATTN_NORM );
					}
				}
				else
				{
					EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/smg1/smg1_dryfire.wav", 1, ATTN_NORM );
				}
			}
			else
			{
				// the first round of the three round burst plays the sound and puts a sound in the world sound list.
				if (m_cAmmoLoaded > 0)
				{
					Shoot();
					
					EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/pistol_fire.wav", 1, ATTN_NORM );
				}
				else
				{
					EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/ar1/ar1_dryfire.wav", 1, ATTN_NORM );
				}
			}

			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );
		}	break;
		case WPOLICE_AE_BURST2:
		case WPOLICE_AE_BURST3:
		{
			// the first round of the three round burst plays the sound and puts a sound in the world sound list.
			if (m_cAmmoLoaded > 0)
			{		
				if( FBitSet( pev->weapons, WPOLICE_SMG1 ) )
				{
					Shoot();
				}
			}
			else
			{
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/smg1/smg1_dryfire.wav", 1, ATTN_NORM );
			}
		}	break;
		case WPOLICE_AE_KICK:
		{
			CBaseEntity *pHurt = Kick();

			if( pHurt )
			{
				// SOUND HERE!
				UTIL_MakeVectors( pev->angles );
				pHurt->pev->punchangle.x = 15;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
				pHurt->TakeDamage( pev, pev, gSkillData.wgruntDmgKick, DMG_CLUB );
			}
		
		}	break;
		case WPOLICE_AE_CAUGHT_ENEMY:
		{
			if( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_ALERT", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
				JustSpoke();
			}
		}	break;
		default:
			CSquadMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CWPolice::Spawn()
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL( ENT( pev ), "models/hgrunt.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if (pev->health == 0)
		pev->health		= gSkillData.wgruntHealth;
	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flNextGrenadeCheck	= gpGlobals->time + 1;
	m_flNextPainTime	= gpGlobals->time;
	m_iSentence		= WPOLICE_SENT_NONE;

	m_afCapability		= bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded		= FALSE;
	m_fFirstEncounter	= TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector( 0, 0, 55 );

	if( pev->weapons == 0 )
	{
		// initialize to original values
		pev->weapons = WPOLICE_PISTOL;
		// pev->weapons = WPOLICE_SMG1 | WPOLICE_GRENADELAUNCHER;
	}

	if( FBitSet( pev->weapons, WPOLICE_SMG1 ) )
	{
		m_cClipSize = WPOLICE_CLIP_SIZE;
	}
	else
	{
		m_cClipSize = WPOLICE_CLIP_SIZE * 0.6;
	}
	m_cAmmoLoaded = m_cClipSize;

	CTalkMonster::g_talkWaitTime = 0;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CWPolice::Precache()
{
	if (pev->model)
		PRECACHE_MODEL(STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL( "models/hgrunt.mdl" );

	PRECACHE_SOUND( "weapons/dryfire1.wav" ); //LRC
	PRECACHE_SOUND( "weapons/smg1/smg1_dryfire.wav" );
	PRECACHE_SOUND( "weapons/ar1/ar1_dryfire.wav" );
	
	PRECACHE_SOUND( "weapons/smg1/smg1_3rounds_1.wav" );
	PRECACHE_SOUND( "weapons/smg1/smg1_3rounds_2.wav" );
	PRECACHE_SOUND( "weapons/smg1/smg1_3rounds_3.wav" );
	
	PRECACHE_SOUND( "weapons/ar1/ar1_fire_1.wav" );
	PRECACHE_SOUND( "weapons/ar1/ar1_fire_2.wav" );
	PRECACHE_SOUND( "weapons/ar1/ar1_fire_3.wav" );
	
	PRECACHE_SOUND( "hgrunt/gr_die1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_die2.wav" );
	PRECACHE_SOUND( "hgrunt/gr_die3.wav" );

	PRECACHE_SOUND( "hgrunt/gr_pain1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain2.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain3.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain4.wav" );
	PRECACHE_SOUND( "hgrunt/gr_pain5.wav" );

	PRECACHE_SOUND( "hgrunt/gr_reload1.wav" );

	PRECACHE_SOUND( "weapons/glauncher.wav" );

	PRECACHE_SOUND( "weapons/pistol_fire.wav" );

	PRECACHE_SOUND( "zombie/claw_miss2.wav" );// because we use the basemonster SWIPE animation event

	// get voice pitch
	if( RANDOM_LONG( 0, 1 ) )
		m_voicePitch = 109 + RANDOM_LONG( 0, 7 );
	else
		m_voicePitch = 100;

	m_iBrassShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell
	m_iShotgunShell = PRECACHE_MODEL( "models/shotgunshell.mdl" );
}

//=========================================================
// start task
//=========================================================
void CWPolice::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch( pTask->iTask )
	{
	case TASK_WPOLICE_CHECK_FIRE:
		if( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_WPOLICE_NOFIRE );
		}
		TaskComplete();
		break;
	case TASK_WPOLICE_SPEAK_SENTENCE:
		SpeakSentence();
		TaskComplete();
		break;
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CSquadMonster::StartTask( pTask );
		break;
	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;
	case TASK_WPOLICE_FACE_TOSS_DIR:
		break;
	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CSquadMonster::StartTask( pTask );
		if( pev->movetype == MOVETYPE_FLY )
		{
			m_IdealActivity = ACT_GLIDE;
		}
		break;
	default: 
		CSquadMonster::StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CWPolice::RunTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_WPOLICE_FACE_TOSS_DIR:
		{
			// project a point along the toss vector and turn to face that point.
			MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
			ChangeYaw( pev->yaw_speed );

			if( FacingIdeal() )
			{
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			}
			break;
		}
	default:
		{
			CSquadMonster::RunTask( pTask );
			break;
		}
	}
}

//=========================================================
// PainSound
//=========================================================
void CWPolice::PainSound( void )
{
	if( gpGlobals->time > m_flNextPainTime )
	{
#if 0
		if( RANDOM_LONG( 0, 99 ) < 5 )
		{
			// pain sentences are rare
			if( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( ENT( pev ), "HG_PAIN", WPOLICE_SENTENCE_VOLUME, ATTN_NORM, 0, PITCH_NORM );
				JustSpoke();
				return;
			}
		}
#endif
		switch( RANDOM_LONG( 0, 6 ) )
		{
		case 0:	
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_pain3.wav", 1, ATTN_NORM );	
			break;
		case 1:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_pain4.wav", 1, ATTN_NORM );	
			break;
		case 2:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_pain5.wav", 1, ATTN_NORM );	
			break;
		case 3:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_pain1.wav", 1, ATTN_NORM );	
			break;
		case 4:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_pain2.wav", 1, ATTN_NORM );	
			break;
		}

		m_flNextPainTime = gpGlobals->time + 1;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CWPolice::DeathSound( void )
{
	switch( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_die1.wav", 1, ATTN_IDLE );	
		break;
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_die2.wav", 1, ATTN_IDLE );	
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "hgrunt/gr_die3.wav", 1, ATTN_IDLE );	
		break;
	}
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// WPoliceFail
//=========================================================
Task_t tlWPoliceFail[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT, (float)2 },
	{ TASK_WAIT_PVS, (float)0 },
};

Schedule_t slWPoliceFail[] =
{
	{
		tlWPoliceFail,
		ARRAYSIZE( tlWPoliceFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"WPolice Fail"
	},
};

//=========================================================
// WPolice Combat Fail
//=========================================================
Task_t tlWPoliceCombatFail[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, (float)2 },
	{ TASK_WAIT_PVS, (float)0 },
};

Schedule_t slWPoliceCombatFail[] =
{
	{
		tlWPoliceCombatFail,
		ARRAYSIZE( tlWPoliceCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"WPolice Combat Fail"
	},
};

//=========================================================
// Victory dance!
//=========================================================
Task_t tlWPoliceVictoryDance[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WAIT, (float)1.5 },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, (float)0 },
	{ TASK_WALK_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
};

Schedule_t slWPoliceVictoryDance[] =
{
	{
		tlWPoliceVictoryDance,
		ARRAYSIZE( tlWPoliceVictoryDance ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE,
		0,
		"WPoliceVictoryDance"
	},
};

//=========================================================
// Establish line of fire - move to a position that allows
// the grunt to attack.
//=========================================================
Task_t tlWPoliceEstablishLineOfFire[] = 
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_WPOLICE_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, (float)0 },
	{ TASK_WPOLICE_SPEAK_SENTENCE,(float)0 },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
};

Schedule_t slWPoliceEstablishLineOfFire[] =
{
	{
		tlWPoliceEstablishLineOfFire,
		ARRAYSIZE( tlWPoliceEstablishLineOfFire ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"WPoliceEstablishLineOfFire"
	},
};

//=========================================================
// WPoliceFoundEnemy - grunt established sight with an enemy
// that was hiding from the squad.
//=========================================================
Task_t tlWPoliceFoundEnemy[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_SIGNAL1 },
};

Schedule_t slWPoliceFoundEnemy[] =
{
	{
		tlWPoliceFoundEnemy,
		ARRAYSIZE( tlWPoliceFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"WPoliceFoundEnemy"
	},
};

//=========================================================
// WPoliceCombatFace Schedule
//=========================================================
Task_t tlWPoliceCombatFace1[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WAIT, (float)1.5 },
	{ TASK_SET_SCHEDULE, (float)SCHED_WPOLICE_SWEEP },
};

Schedule_t slWPoliceCombatFace[] =
{
	{
		tlWPoliceCombatFace1,
		ARRAYSIZE( tlWPoliceCombatFace1 ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Combat Face"
	},
};

//=========================================================
// Suppressing fire - don't stop shooting until the clip is
// empty or grunt gets hurt.
//=========================================================
Task_t tlWPoliceSignalSuppress[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, (float)0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0},
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
};

Schedule_t slWPoliceSignalSuppress[] =
{
	{
		tlWPoliceSignalSuppress,
		ARRAYSIZE( tlWPoliceSignalSuppress ),
		bits_COND_ENEMY_DEAD |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_WPOLICE_NOFIRE |
		bits_COND_NO_AMMO_LOADED,
		bits_SOUND_DANGER,
		"SignalSuppress"
	},
};

Task_t tlWPoliceSuppress[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
};

Schedule_t slWPoliceSuppress[] =
{
	{
		tlWPoliceSuppress,
		ARRAYSIZE( tlWPoliceSuppress ),
		bits_COND_ENEMY_DEAD |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_WPOLICE_NOFIRE |
		bits_COND_NO_AMMO_LOADED,
		bits_SOUND_DANGER,
		"Suppress"
	},
};

//=========================================================
// grunt wait in cover - we don't allow danger or the ability
// to attack to break a grunt's run to cover schedule, but
// when a grunt is in cover, we do want them to attack if they can.
//=========================================================
Task_t tlWPoliceWaitInCover[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, (float)1 },
};

Schedule_t slWPoliceWaitInCover[] =
{
	{
		tlWPoliceWaitInCover,
		ARRAYSIZE( tlWPoliceWaitInCover ),
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		bits_SOUND_DANGER,
		"WPoliceWaitInCover"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t tlWPoliceTakeCover1[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_WPOLICE_TAKECOVER_FAILED },
	{ TASK_WAIT, (float)0.2	 },
	{ TASK_FIND_COVER_FROM_ENEMY, (float)0 },
	{ TASK_WPOLICE_SPEAK_SENTENCE, (float)0 },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_REMEMBER, (float)bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float)SCHED_WPOLICE_WAIT_FACE_ENEMY	},
};

Schedule_t slWPoliceTakeCover[] =
{
	{ 
		tlWPoliceTakeCover1,
		ARRAYSIZE ( tlWPoliceTakeCover1 ), 
		0,
		0,
		"TakeCover"
	},
};

//=========================================================
// drop grenade then run to cover.
//=========================================================
Task_t tlWPoliceGrenadeCover1[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FIND_COVER_FROM_ENEMY, (float)99 },
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, (float)384 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, (float)0 },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_SET_SCHEDULE, (float)SCHED_WPOLICE_WAIT_FACE_ENEMY },
};

Schedule_t slWPoliceGrenadeCover[] =
{
	{
		tlWPoliceGrenadeCover1,
		ARRAYSIZE( tlWPoliceGrenadeCover1 ),
		0,
		0,
		"GrenadeCover"
	},
};

//=========================================================
// drop grenade then run to cover.
//=========================================================
Task_t tlWPoliceTossGrenadeCover1[] =
{
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_RANGE_ATTACK2, (float)0 },
	{ TASK_SET_SCHEDULE, (float)SCHED_TAKE_COVER_FROM_ENEMY },
};

Schedule_t slWPoliceTossGrenadeCover[] =
{
	{
		tlWPoliceTossGrenadeCover1,
		ARRAYSIZE( tlWPoliceTossGrenadeCover1 ),
		0,
		0,
		"TossGrenadeCover"
	},
};

//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t tlWPoliceTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_COWER },// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, (float)0 },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_REMEMBER, (float)bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, (float)179 },
};

Schedule_t slWPoliceTakeCoverFromBestSound[] =
{
	{
		tlWPoliceTakeCoverFromBestSound,
		ARRAYSIZE( tlWPoliceTakeCoverFromBestSound ),
		0,
		0,
		"WPoliceTakeCoverFromBestSound"
	},
};

//=========================================================
// WPolice reload schedule
//=========================================================
Task_t	tlWPoliceHideReload[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, (float)0 },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_REMEMBER, (float)bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_RELOAD },
};

Schedule_t slWPoliceHideReload[] =
{
	{
		tlWPoliceHideReload,
		ARRAYSIZE( tlWPoliceHideReload ),
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"WPoliceHideReload"
	}
};

//=========================================================
// Do a turning sweep of the area
//=========================================================
Task_t tlWPoliceSweep[] =
{
	{ TASK_TURN_LEFT, (float)179 },
	{ TASK_WAIT, (float)1 },
	{ TASK_TURN_LEFT, (float)179 },
	{ TASK_WAIT, (float)1 },
};

Schedule_t slWPoliceSweep[] =
{
	{
		tlWPoliceSweep,
		ARRAYSIZE( tlWPoliceSweep ),
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_HEAR_SOUND,
		bits_SOUND_WORLD |// sound flags
		bits_SOUND_DANGER |
		bits_SOUND_PLAYER,
		"WPolice Sweep"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t tlWPoliceRangeAttack1A[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
};

Schedule_t slWPoliceRangeAttack1A[] =
{
	{
		tlWPoliceRangeAttack1A,
		ARRAYSIZE( tlWPoliceRangeAttack1A ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAR_SOUND |
		bits_COND_WPOLICE_NOFIRE |
		bits_COND_NO_AMMO_LOADED,
		bits_SOUND_DANGER,
		"Range Attack1A"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t tlWPoliceRangeAttack1B[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_IDLE_ANGRY },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_WPOLICE_CHECK_FIRE, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
};

Schedule_t slWPoliceRangeAttack1B[] =
{
	{
		tlWPoliceRangeAttack1B,
		ARRAYSIZE( tlWPoliceRangeAttack1B ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NO_AMMO_LOADED |
		bits_COND_WPOLICE_NOFIRE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Range Attack1B"
	},
};

//=========================================================
// secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t tlWPoliceRangeAttack2[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_WPOLICE_FACE_TOSS_DIR, (float)0 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float)SCHED_WPOLICE_WAIT_FACE_ENEMY },// don't run immediately after throwing grenade.
};

Schedule_t slWPoliceRangeAttack2[] =
{
	{
		tlWPoliceRangeAttack2,
		ARRAYSIZE( tlWPoliceRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	},
};

//=========================================================
// repel 
//=========================================================
Task_t tlWPoliceRepel[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_IDEAL, (float)0 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_GLIDE },
};

Schedule_t	slWPoliceRepel[] =
{
	{
		tlWPoliceRepel,
		ARRAYSIZE( tlWPoliceRepel ),
		bits_COND_SEE_ENEMY |
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER |
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER, 
		"Repel"
	},
};

//=========================================================
// repel 
//=========================================================
Task_t tlWPoliceRepelAttack[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_FLY },
};

Schedule_t slWPoliceRepelAttack[] =
{
	{
		tlWPoliceRepelAttack,
		ARRAYSIZE( tlWPoliceRepelAttack ),
		bits_COND_ENEMY_OCCLUDED,
		0,
		"Repel Attack"
	},
};

//=========================================================
// repel land
//=========================================================
Task_t tlWPoliceRepelLand[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_PLAY_SEQUENCE, (float)ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, (float)0 },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_CLEAR_LASTPOSITION, (float)0 },
};

Schedule_t slWPoliceRepelLand[] =
{
	{
		tlWPoliceRepelLand,
		ARRAYSIZE( tlWPoliceRepelLand ),
		bits_COND_SEE_ENEMY |
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER |
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER, 
		"Repel Land"
	},
};


DEFINE_CUSTOM_SCHEDULES( CWPolice )
{
	slWPoliceFail,
	slWPoliceCombatFail,
	slWPoliceVictoryDance,
	slWPoliceEstablishLineOfFire,
	slWPoliceFoundEnemy,
	slWPoliceCombatFace,
	slWPoliceSignalSuppress,
	slWPoliceSuppress,
	slWPoliceWaitInCover,
	slWPoliceTakeCover,
	slWPoliceGrenadeCover,
	slWPoliceTossGrenadeCover,
	slWPoliceTakeCoverFromBestSound,
	slWPoliceHideReload,
	slWPoliceSweep,
	slWPoliceRangeAttack1A,
	slWPoliceRangeAttack1B,
	slWPoliceRangeAttack2,
	slWPoliceRepel,
	slWPoliceRepelAttack,
	slWPoliceRepelLand,
};

IMPLEMENT_CUSTOM_SCHEDULES( CWPolice, CSquadMonster )

//=========================================================
// SetActivity 
//=========================================================
void CWPolice::SetActivity( Activity NewActivity )
{
	int iSequence = ACTIVITY_NOT_AVAILABLE;
	//void *pmodel = GET_MODEL_PTR( ENT( pev ) );

	switch( NewActivity )
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if( !FBitSet( pev->weapons, WPOLICE_SMG1 ) )
		{
			if( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_mp5" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_mp5" );
			}
		}
		else
		{
			if( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_pistol" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_pistol" );
			}
		}
		break;
	case ACT_RANGE_ATTACK2:
		// grunt is going to a secondary long range attack. This may be a thrown 
		// grenade or fired grenade, we must determine which and pick proper sequence
		if( pev->weapons & WPOLICE_FRAG )
		{
			// get toss anim
			iSequence = LookupSequence( "throwgrenade" );
		}
		// LRC: added a test to stop a marine without a launcher from firing.
		else if ( pev->weapons & WPOLICE_GRENADELAUNCHER )
		{
			// get launch anim
			iSequence = LookupSequence( "launchgrenade" );
		}
		else
		{
			ALERT( at_console, "No grenades available. "); // flow into the error message we get at the end...
		}
		break;
	case ACT_RUN:
		if( pev->health <= WPOLICE_LIMP_HEALTH )
		{
			// limp!
			iSequence = LookupActivity( ACT_RUN_HURT );
		}
		else
		{
			iSequence = LookupActivity( NewActivity );
		}
		break;
	case ACT_WALK:
		if( pev->health <= WPOLICE_LIMP_HEALTH )
		{
			// limp!
			iSequence = LookupActivity( ACT_WALK_HURT );
		}
		else
		{
			iSequence = LookupActivity( NewActivity );
		}
		break;
	case ACT_IDLE:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			NewActivity = ACT_IDLE_ANGRY;
		}
		iSequence = LookupActivity( NewActivity );
		break;
	default:
		iSequence = LookupActivity( NewActivity );
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), NewActivity );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// Get Schedule!
//=========================================================
Schedule_t *CWPolice::GetSchedule( void )
{

	// clear old sentence
	m_iSentence = WPOLICE_SENT_NONE;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if( pev->flags & FL_ONGROUND )
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_WPOLICE_REPEL_LAND );
		}
		else
		{
			// repel down a rope, 
			if( m_MonsterState == MONSTERSTATE_COMBAT )
				return GetScheduleOfType( SCHED_WPOLICE_REPEL_ATTACK );
			else
				return GetScheduleOfType( SCHED_WPOLICE_REPEL );
		}
	}

	// grunts place HIGH priority on running away from danger sounds.
	if( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if( pSound )
		{
			if( pSound->m_iType & bits_SOUND_DANGER )
			{
				// dangerous sound nearby!

				//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
				// and the grunt should find cover from the blast
				// good place for "SHIT!" or some other colorful verbal indicator of dismay.
				// It's not safe to play a verbal order here "Scatter", etc cause 
				// this may only affect a single individual in a squad. 
				if( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( ENT( pev ), "HG_GREN", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
			/*
			if( !HasConditions( bits_COND_SEE_ENEMY ) && ( pSound->m_iType & ( bits_SOUND_PLAYER | bits_SOUND_COMBAT ) ) )
			{
				MakeIdealYaw( pSound->m_vecOrigin );
			}
			*/
		}
	}
	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
			// dead enemy
			if( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster::GetSchedule();
			}

			// new enemy
			if( HasConditions( bits_COND_NEW_ENEMY ) )
			{
				if( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if( !IsLeader() )
					{
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
					}
					else 
					{
						//!!!KELLY - the leader of a squad of grunts has just seen the player or a 
						// monster and has made it the squad's enemy. You
						// can check pev->flags for FL_CLIENT to determine whether this is the player
						// or a monster. He's going to immediately start
						// firing, though. If you'd like, we can make an alternate "first sight" 
						// schedule where the leader plays a handsign anim
						// that gives us enough time to hear a short sentence or spoken command
						// before he starts pluggin away.
						if( FOkToSpeak() )// && RANDOM_LONG( 0, 1 ) )
						{
							if( ( m_hEnemy != 0 ) && m_hEnemy->IsPlayer() )
								// player
								SENTENCEG_PlayRndSz( ENT( pev ), "HG_ALERT", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
							else if( ( m_hEnemy != 0 ) &&
									( m_hEnemy->Classify() != CLASS_PLAYER_ALLY ) && 
									( m_hEnemy->Classify() != CLASS_CITIZEN_PASSIVE ) && 
									( m_hEnemy->Classify() != CLASS_CITIZEN_MILITARY ) && 
									( m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE ) && 
									( m_hEnemy->Classify() != CLASS_MACHINE ) )
								// monster
								SENTENCEG_PlayRndSz( ENT( pev ), "HG_MONST", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );

							JustSpoke();
						}

						if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType( SCHED_WPOLICE_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType( SCHED_WPOLICE_ESTABLISH_LINE_OF_FIRE );
						}
					}
				}
			}
			// no ammo
			else if( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				return GetScheduleOfType( SCHED_WPOLICE_COVER_AND_RELOAD );
			}
			// damaged just a little
			else if( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{
				// if hurt:
				// 90% chance of taking cover
				// 10% chance of flinch.
				int iPercent = RANDOM_LONG( 0, 99 );

				if( iPercent <= 90 && m_hEnemy != 0 )
				{
					// only try to take cover if we actually have an enemy!

					//!!!KELLY - this grunt was hit and is going to run to cover.
					if( FOkToSpeak() ) // && RANDOM_LONG( 0, 1 ) )
					{
						//SENTENCEG_PlayRndSz( ENT( pev ), "HG_COVER", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
						m_iSentence = WPOLICE_SENT_COVER;
						//JustSpoke();
					}
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				else
				{
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
				}
			}
			// can kick
			else if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
			}
			// can grenade launch
			else if( FBitSet( pev->weapons, WPOLICE_GRENADELAUNCHER) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_WGRUNT_GRENADE ) )
			{
				// shoot a grenade if you can
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
			// can shoot
			else if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if( InSquad() )
				{
					// if the enemy has eluded the squad and a squad member has just located the enemy
					// and the enemy does not see the squad member, issue a call to the squad to waste a 
					// little time and give the player a chance to turn.
					if( MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
					{
						MySquadLeader()->m_fEnemyEluded = FALSE;
						return GetScheduleOfType( SCHED_WPOLICE_FOUND_ENEMY );
					}
				}

				if( OccupySlot( bits_SLOTS_WGRUNT_ENGAGE ) )
				{
					// try to take an available ENGAGE slot
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				else if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_WGRUNT_GRENADE ) )
				{
					// throw a grenade if can and no engage slots are available
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else
				{
					// hide!
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
			// can't see enemy
			else if( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_WGRUNT_GRENADE ) )
				{
					//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
					if( FOkToSpeak() )
					{
						SENTENCEG_PlayRndSz( ENT( pev ), "HG_THROW", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else if( OccupySlot( bits_SLOTS_WGRUNT_ENGAGE ) )
				{
					//!!!KELLY - grunt cannot see the enemy and has just decided to 
					// charge the enemy's position. 
					if( FOkToSpeak() )// && RANDOM_LONG( 0, 1 ) )
					{
						//SENTENCEG_PlayRndSz( ENT( pev ), "HG_CHARGE", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
						m_iSentence = WPOLICE_SENT_CHARGE;
						//JustSpoke();
					}

					return GetScheduleOfType( SCHED_WPOLICE_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					//!!!KELLY - grunt is going to stay put for a couple seconds to see if
					// the enemy wanders back out into the open, or approaches the
					// grunt's covered position. Good place for a taunt, I guess?
					if( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
					{
						SENTENCEG_PlayRndSz( ENT( pev ), "HG_TAUNT", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}

			if( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType( SCHED_WPOLICE_ESTABLISH_LINE_OF_FIRE );
			}
		}
		break;
	default:
		break;
	}

	// no special cases here, call the base class
	return CSquadMonster::GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t *CWPolice::GetScheduleOfType( int Type ) 
{
	switch( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			if( InSquad() )
			{
				if( g_iSkillLevel == SKILL_HARD && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_WGRUNT_GRENADE ) )
				{
					if( FOkToSpeak() )
					{
						SENTENCEG_PlayRndSz( ENT( pev ), "HG_THROW", WPOLICE_SENTENCE_VOLUME, WPOLICE_ATTN, 0, m_voicePitch );
						JustSpoke();
					}
					return slWPoliceTossGrenadeCover;
				}
				else
				{
					return &slWPoliceTakeCover[0];
				}
			}
			else
			{
				if ( OccupySlot( bits_SLOTS_WGRUNT_GRENADE ) && RANDOM_LONG(0,1) )
				{
					return &slWPoliceGrenadeCover[ 0 ];
				}
				else
				{
					return &slWPoliceTakeCover[ 0 ];
				}
			}
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slWPoliceTakeCoverFromBestSound[0];
		}
	case SCHED_WPOLICE_TAKECOVER_FAILED:
		{
			if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_WGRUNT_ENGAGE ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}

			return GetScheduleOfType( SCHED_FAIL );
		}
		break;
	case SCHED_WPOLICE_ELOF_FAIL:
		{
			// human grunt is unable to move to a position that allows him to attack the enemy.
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}
		break;
	case SCHED_WPOLICE_ESTABLISH_LINE_OF_FIRE:
		{
			return &slWPoliceEstablishLineOfFire[0];
		}
		break;
	case SCHED_RANGE_ATTACK1:
		{
			// randomly stand or crouch
			if( RANDOM_LONG( 0, 9 ) == 0 )
				m_fStanding = RANDOM_LONG( 0, 1 );

			if( m_fStanding )
				return &slWPoliceRangeAttack1B[0];
			else
				return &slWPoliceRangeAttack1A[0];
		}
	case SCHED_RANGE_ATTACK2:
		{
			return &slWPoliceRangeAttack2[0];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slWPoliceCombatFace[0];
		}
	case SCHED_WPOLICE_WAIT_FACE_ENEMY:
		{
			return &slWPoliceWaitInCover[0];
		}
	case SCHED_WPOLICE_SWEEP:
		{
			return &slWPoliceSweep[0];
		}
	case SCHED_WPOLICE_COVER_AND_RELOAD:
		{
			return &slWPoliceHideReload[0];
		}
	case SCHED_WPOLICE_FOUND_ENEMY:
		{
			return &slWPoliceFoundEnemy[0];
		}
	case SCHED_VICTORY_DANCE:
		{
			if( InSquad() )
			{
				if( !IsLeader() )
				{
					return &slWPoliceFail[0];
				}
			}

			return &slWPoliceVictoryDance[0];
		}
	case SCHED_WPOLICE_SUPPRESS:
		{
			if( m_hEnemy->IsPlayer() && m_fFirstEncounter )
			{
				m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				return &slWPoliceSignalSuppress[0];
			}
			else
			{
				return &slWPoliceSuppress[0];
			}
		}
	case SCHED_FAIL:
		{
			if( m_hEnemy != 0 )
			{
				// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slWPoliceCombatFail[0];
			}

			return &slWPoliceFail[0];
		}
	case SCHED_WPOLICE_REPEL:
		{
			if( pev->velocity.z > -128 )
				pev->velocity.z -= 32;
			return &slWPoliceRepel[0];
		}
	case SCHED_WPOLICE_REPEL_ATTACK:
		{
			if( pev->velocity.z > -128 )
				pev->velocity.z -= 32;
			return &slWPoliceRepelAttack[0];
		}
	case SCHED_WPOLICE_REPEL_LAND:
		{
			return &slWPoliceRepelLand[0];
		}
	default:
		{
			return CSquadMonster::GetScheduleOfType( Type );
		}
	}
}

//=========================================================
// CWPoliceRepel - when triggered, spawns a monster_human_grunt
// repelling down a line.
//=========================================================

class CWPoliceRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( monster_wpolice_repel, CWPoliceRepel )

void CWPoliceRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	SetUse( &CWPoliceRepel::RepelUse );
}

void CWPoliceRepel::Precache( void )
{
	UTIL_PrecacheOther( "monster_horde_police" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/rope.spr" );
}

void CWPoliceRepel::RepelUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096.0 ), dont_ignore_monsters, ENT( pev ), &tr );
	/*
	if( tr.pHit && Instance( tr.pHit )->pev->solid != SOLID_BSP )
		return NULL;
	*/

	CBaseEntity *pEntity = Create( "monster_horde_police", pev->origin, pev->angles );
	CBaseMonster *pWPolice = pEntity->MyMonsterPointer();
	pWPolice->pev->movetype = MOVETYPE_FLY;
	pWPolice->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
	pWPolice->SetActivity( ACT_GLIDE );
	// UNDONE: position?
	pWPolice->m_vecLastPosition = tr.vecEndPos;

	CBeam *pBeam = CBeam::BeamCreate( "sprites/rope.spr", 10 );
	pBeam->PointEntInit( pev->origin + Vector( 0, 0, 112 ), pWPolice->entindex() );
	pBeam->SetFlags( BEAM_FSOLID );
	pBeam->SetColor( 255, 255, 255 );
	pBeam->SetThink( &CBeam::SUB_Remove );
	pBeam->SetNextThink( -4096.0f * tr.flFraction / pWPolice->pev->velocity.z + 0.5f );

	UTIL_Remove( this );
}

//=========================================================
// DEAD WPOLICE PROP
//=========================================================
class CDeadWPolice : public CBaseMonster
{
public:
	void Spawn( void );
	int Classify( void ) { return CLASS_WARRIOR_MILITARY; }

	void KeyValue( KeyValueData *pkvd );

	int m_iPose;// which sequence to display	-- temporary, don't need to save
	static const char *m_szPoses[3];
};

const char *CDeadWPolice::m_szPoses[] = { "deadstomach", "deadside", "deadsitting" };

void CDeadWPolice::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "pose" ) )
	{
		m_iPose = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_wpolice_dead, CDeadWPolice )

//=========================================================
// ********** DeadWPolice SPAWN **********
//=========================================================
void CDeadWPolice::Spawn( void )
{
	int oldBody;

	PRECACHE_MODEL( "models/hgrunt.mdl" );
	SET_MODEL( ENT( pev ), "models/hgrunt.mdl" );

	pev->effects		= 0;
	pev->yaw_speed		= 8;
	pev->sequence		= 0;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );

	if( pev->sequence == -1 )
	{
		ALERT( at_console, "Dead wpolice with bad pose\n" );
	}

	// Corpses have less health
	pev->health = 8;

	switch( pev->weapons )
	{
	case 0: // MP5K
		SetBodygroup( GUN_GROUP, GUN_SMG1 );
		break;
	case 1: // 
		SetBodygroup( GUN_GROUP, GUN_PISTOL );
		break;
	case 3: // No gun
		SetBodygroup( GUN_GROUP, GUN_NONE );
		break;
	}

	MonsterInitDead();
}