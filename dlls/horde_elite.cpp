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
// welite - Warlike. Plasma shootin' Horde Elite
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"squadmonster.h"
#include	"weapons.h"
#include	"effects.h"
#include	"soundent.h"
#include	"hornet.h"
#include	"scripted.h"

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_WELITE_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_WELITE_THREAT_DISPLAY
};

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_WELITE_SETUP_HIDE_ATTACK = LAST_COMMON_TASK + 1,
	TASK_WELITE_GET_PATH_TO_ENEMY_CORPSE
};

int iweliteMuzzleFlash;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		WELITE_AE_PLASMA1	( 1 )
#define		WELITE_AE_PLASMA2	( 2 )
#define		WELITE_AE_PLASMA3	( 3 )
#define		WELITE_AE_PLASMA4	( 4 )
#define		WELITE_AE_PLASMA5	( 5 )

#define		WELITE_AE_LEFT_FOOT	 ( 10 )
#define		WELITE_AE_RIGHT_FOOT ( 11 )

#define		WELITE_AE_LEFT_PUNCH ( 12 )
#define		WELITE_AE_RIGHT_PUNCH ( 13 )
#define		WELITE_AE_SHIELD_BASH ( 14 )

#define		WELITE_MELEE_DIST	100.0f

//LRC - body definitions for the welite model
#define		WELITE_BODY_HASGUN 0
#define		WELITE_BODY_NOGUN  1

class CWElite : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int Classify( void );
	int ISoundMask( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -32.0f, -32.0f, 0.0f );
		pev->absmax = pev->origin + Vector( 32.0f, 32.0f, 85.0f );
	}

	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	BOOL FCanCheckAttacks( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	void StartTask( Task_t *pTask );
	void AlertSound( void );
	void DeathSound( void );
	void PainSound( void );
	void AttackSound( void );
	void PrescheduleThink( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int IRelationship( CBaseEntity *pTarget );
	void StopTalking( void );
	BOOL ShouldSpeak( void );
	CUSTOM_SCHEDULES
	virtual void Killed( entvars_t *pevAttacker, int iGib );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];
	static const char *pAttackSounds[];
	static const char *pDieSounds[];
	static const char *pPainSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];

	BOOL m_fCanPlasmaAttack;
	float m_flNextPlasmaAttackCheck;

	float m_flNextPainTime;

	// three hacky fields for speech stuff. These don't really need to be saved.
	float m_flNextSpeakTime;
	float m_flNextWordTime;
	int m_iLastWord;
};

LINK_ENTITY_TO_CLASS( monster_horde_elite, CWElite )

TYPEDESCRIPTION	CWElite::m_SaveData[] =
{
	DEFINE_FIELD( CWElite, m_fCanPlasmaAttack, FIELD_BOOLEAN ),
	DEFINE_FIELD( CWElite, m_flNextPlasmaAttackCheck, FIELD_TIME ),
	DEFINE_FIELD( CWElite, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( CWElite, m_flNextSpeakTime, FIELD_TIME ),
	DEFINE_FIELD( CWElite, m_flNextWordTime, FIELD_TIME ),
	DEFINE_FIELD( CWElite, m_iLastWord, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWElite, CSquadMonster )

const char *CWElite::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CWElite::pAttackMissSounds[] =
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CWElite::pAttackSounds[] =
{
	"welite/ag_attack1.wav",
	"welite/ag_attack2.wav",
	"welite/ag_attack3.wav",
};

const char *CWElite::pDieSounds[] =
{
	"welite/ag_die1.wav",
	"welite/ag_die4.wav",
	"welite/ag_die5.wav",
};

const char *CWElite::pPainSounds[] =
{
	"welite/ag_pain1.wav",
	"welite/ag_pain2.wav",
	"welite/ag_pain3.wav",
	"welite/ag_pain4.wav",
	"welite/ag_pain5.wav",
};

const char *CWElite::pIdleSounds[] =
{
	"welite/ag_idle1.wav",
	"welite/ag_idle2.wav",
	"welite/ag_idle3.wav",
	"welite/ag_idle4.wav",
};

const char *CWElite::pAlertSounds[] =
{
	"welite/ag_alert1.wav",
	"welite/ag_alert3.wav",
	"welite/ag_alert4.wav",
	"welite/ag_alert5.wav",
};

//=========================================================
// IRelationship - overridden because Human Grunts are 
// Horde Elite's nemesis.
//=========================================================
int CWElite::IRelationship( CBaseEntity *pTarget )
{
	if( FClassnameIs( pTarget->pev, "monster_conscript" ) )
	{
		return R_NM;
	}

	return CSquadMonster::IRelationship( pTarget );
}

//=========================================================
// ISoundMask 
//=========================================================
int CWElite::ISoundMask( void )
{
	return ( bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER );
}

//=========================================================
// TraceAttack
//=========================================================
void CWElite::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( ptr->iHitgroup == 10 && ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB ) ) )
	{
		// hit armor
		if( pev->dmgtime != gpGlobals->time || ( RANDOM_LONG( 0, 10 ) < 1 ) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1.0f, 2.0f ) );
			pev->dmgtime = gpGlobals->time;
		}

		if( RANDOM_LONG( 0, 1 ) == 0 )
		{
			Vector vecTracerDir = vecDir;

			vecTracerDir.x += RANDOM_FLOAT( -0.3f, 0.3f );
			vecTracerDir.y += RANDOM_FLOAT( -0.3f, 0.3f );
			vecTracerDir.z += RANDOM_FLOAT( -0.3f, 0.3f );

			vecTracerDir = vecTracerDir * -512.0f;

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
			WRITE_BYTE( TE_TRACER );
				WRITE_COORD( ptr->vecEndPos.x );
				WRITE_COORD( ptr->vecEndPos.y );
				WRITE_COORD( ptr->vecEndPos.z );

				WRITE_COORD( vecTracerDir.x );
				WRITE_COORD( vecTracerDir.y );
				WRITE_COORD( vecTracerDir.z );
			MESSAGE_END();
		}

		flDamage -= 20.0f;
		if( flDamage <= 0.0f )
			flDamage = 0.1f;// don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated
	}
	else
	{
		SpawnBlood( ptr->vecEndPos, BloodColor(), flDamage );// a little surface blood.
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
	}

	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

//=========================================================
// StopTalking - won't speak again for 10-20 seconds.
//=========================================================
void CWElite::StopTalking( void )
{
	m_flNextWordTime = m_flNextSpeakTime = gpGlobals->time + 10.0f + RANDOM_LONG( 0, 10 );
}

//=========================================================
// ShouldSpeak - Should this welite be talking?
//=========================================================
BOOL CWElite::ShouldSpeak( void )
{
	if( m_flNextSpeakTime > gpGlobals->time )
	{
		// my time to talk is still in the future.
		return FALSE;
	}

	if( pev->spawnflags & SF_MONSTER_GAG )
	{
		if( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			// if gagged, don't talk outside of combat.
			// if not going to talk because of this, put the talk time 
			// into the future a bit, so we don't talk immediately after 
			// going into combat
			m_flNextSpeakTime = gpGlobals->time + 3.0f;
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
// PrescheduleThink 
//=========================================================
void CWElite::PrescheduleThink( void )
{
	if( ShouldSpeak() )
	{
		if( m_flNextWordTime < gpGlobals->time )
		{
			int num = -1;

			do
			{
				num = RANDOM_LONG( 0, ARRAYSIZE( pIdleSounds ) - 1 );
			} while( num == m_iLastWord );

			m_iLastWord = num;

			// play a new sound
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, pIdleSounds[num], 1.0f, ATTN_NORM );

			// is this word our last?
			if( RANDOM_LONG( 1, 10 ) <= 1 )
			{
				// stop talking.
				StopTalking();
			}
			else
			{
				m_flNextWordTime = gpGlobals->time + RANDOM_FLOAT( 0.5f, 1.0f );
			}
		}
	}
}

//=========================================================
// DieSound
//=========================================================
void CWElite::DeathSound( void )
{
	StopTalking();

	EMIT_SOUND( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pDieSounds ), 1.0f, ATTN_NORM );
}

//=========================================================
// AlertSound
//=========================================================
void CWElite::AlertSound( void )
{
	StopTalking();

	EMIT_SOUND( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), 1.0f, ATTN_NORM );
}

//=========================================================
// AttackSound
//=========================================================
void CWElite::AttackSound( void )
{
	StopTalking();

	EMIT_SOUND( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0f, ATTN_NORM );
}

//=========================================================
// PainSound
//=========================================================
void CWElite::PainSound( void )
{
	if( m_flNextPainTime > gpGlobals->time )
	{
		return;
	}

	m_flNextPainTime = gpGlobals->time + 0.6f;

	StopTalking();

	EMIT_SOUND( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0f, ATTN_NORM );
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CWElite::Classify( void )
{
	return m_iClass?m_iClass:CLASS_WARRIOR_MILITARY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CWElite::SetYawSpeed( void )
{
	int ys;

	switch( m_Activity )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 110;
		break;
	default:
		ys = 100;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CWElite::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case WELITE_AE_PLASMA1:
	case WELITE_AE_PLASMA2:
	case WELITE_AE_PLASMA3:
	case WELITE_AE_PLASMA4:
	case WELITE_AE_PLASMA5:
		{
			// m_vecEnemyLKP should be center of enemy body
			Vector vecArmPos, vecArmDir;
			Vector vecDirToEnemy;
			Vector angDir;

			if( HasConditions( bits_COND_SEE_ENEMY ) )
			{
				vecDirToEnemy = ( ( m_vecEnemyLKP ) - pev->origin );
				angDir = UTIL_VecToAngles( vecDirToEnemy );
				vecDirToEnemy = vecDirToEnemy.Normalize();
			}
			else
			{
				angDir = pev->angles;
				UTIL_MakeAimVectors( angDir );
				vecDirToEnemy = gpGlobals->v_forward;
			}

			pev->effects = EF_MUZZLEFLASH;
			
			/*
			// make angles +-180
			if( angDir.x > 180.0f )
			{
				angDir.x = angDir.x - 360.0f;
			}
			*/
			
			angDir.x = -angDir.x + RANDOM_LONG( -1, 3 );
			angDir.y += 1.5f;

			SetBlending( 0, angDir.x );
			GetAttachment( 0, vecArmPos, vecArmDir );
			
			if ( RANDOM_LONG( 0, 2 ) == 1 )
			{
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/electro4.wav", 1, ATTN_NORM, 0, 90 );
			}
			else if ( RANDOM_LONG( 0, 1 ) == 1 )
			{
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/electro5.wav", 1, ATTN_NORM, 0, 90 );
			}
			else
			{
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/electro6.wav", 1, ATTN_NORM, 0, 90 );
			}

			vecArmPos = vecArmPos + vecDirToEnemy * 32.0f;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecArmPos );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( vecArmPos.x );	// pos
				WRITE_COORD( vecArmPos.y );	
				WRITE_COORD( vecArmPos.z );	
				WRITE_SHORT( iweliteMuzzleFlash );		// model
				WRITE_BYTE( 6 );				// size * 10
				WRITE_BYTE( 128 );			// brightness
			MESSAGE_END();
			
			CBaseMonster *pPlasma = (CBaseMonster*)Create( "welite_plasma", vecArmPos, angDir, edict() );
			UTIL_MakeVectors( pPlasma->pev->angles );
			pPlasma->pev->velocity = gpGlobals->v_forward * ( 1200.0f * RANDOM_LONG( 1, 2 ) );
		}
		break;
	case WELITE_AE_LEFT_FOOT:
		switch( RANDOM_LONG( 0, 1 ) )
		{
		// left foot
		case 0:
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "player/pl_ladder2.wav", 1, ATTN_NORM, 0, 70 );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "player/pl_ladder4.wav", 1, ATTN_NORM, 0, 70 );
			break;
		}
		break;
	case WELITE_AE_RIGHT_FOOT:
		// right foot
		switch( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "player/pl_ladder1.wav", 1, ATTN_NORM, 0, 70 );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "player/pl_ladder3.wav", 1, ATTN_NORM, 0, 70 );
			break;
		}
		break;

	case WELITE_AE_LEFT_PUNCH:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( WELITE_MELEE_DIST, gSkillData.weliteDmgPunch, DMG_CLUB );

			if( pHurt )
			{
				pHurt->pev->punchangle.y = -25.0f;
				pHurt->pev->punchangle.x = 8.0f;

				// OK to use gpGlobals without calling MakeVectors, cause CheckTraceHullAttack called it above.
				if( pHurt->IsPlayer() )
				{
					// this is a player. Knock him around.
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 250.0f;
				}

				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0f, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );

				Vector vecArmPos, vecArmAng;
				GetAttachment( 0, vecArmPos, vecArmAng );
				SpawnBlood( vecArmPos, pHurt->BloodColor(), 25 );// a little surface blood.
			}
			else
			{
				// Play a random attack miss sound
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0f, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}
		}
		break;
	case WELITE_AE_RIGHT_PUNCH:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( WELITE_MELEE_DIST, gSkillData.weliteDmgPunch, DMG_CLUB );

			if( pHurt )
			{
				pHurt->pev->punchangle.y = 25.0f;
				pHurt->pev->punchangle.x = 8.0f;

				// OK to use gpGlobals without calling MakeVectors, cause CheckTraceHullAttack called it above.
				if( pHurt->IsPlayer() )
				{
					// this is a player. Knock him around.
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * -250.0f;
				}

				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0f, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );

				Vector vecArmPos, vecArmAng;
				GetAttachment( 0, vecArmPos, vecArmAng );
				SpawnBlood( vecArmPos, pHurt->BloodColor(), 25 );// a little surface blood.
			}
			else
			{
				// Play a random attack miss sound
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0f, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}
		}
		break;
	default:
		CSquadMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CWElite::Spawn()
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL( ENT( pev ), "models/welite.mdl" );
	UTIL_SetSize( pev, Vector( -32.0f, -32.0f, 0.0f ), Vector( 32.0f, 32.0f, 64.0f ) );

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->effects = 0;

	if (pev->health == 0)
		pev->health = gSkillData.weliteHealth;
	m_flFieldOfView = 0.2f;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = 0;
	m_afCapability |= bits_CAP_SQUAD;

	m_HackedGunPos = Vector( 24.0f, 64.0f, 48.0f );

	m_flNextSpeakTime = m_flNextWordTime = gpGlobals->time + 10.0f + RANDOM_LONG( 0, 10 );

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CWElite::Precache()
{
	if (pev->model)
		PRECACHE_MODEL(STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL( "models/welite.mdl" );

	PRECACHE_SOUND_ARRAY( pAttackHitSounds );
	PRECACHE_SOUND_ARRAY( pAttackMissSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pDieSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );

	PRECACHE_SOUND( "weapons/electro4.wav" );
	PRECACHE_SOUND( "weapons/electro5.wav" );
	PRECACHE_SOUND( "weapons/electro6.wav" );
	

	iweliteMuzzleFlash = PRECACHE_MODEL( "sprites/muz4.spr" );

	UTIL_PrecacheOther( "welite_plasma" );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// Fail Schedule
//=========================================================
Task_t tlWEliteFail[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT, 2.0f },
	{ TASK_WAIT_PVS, 0.0f },
};

Schedule_t slWEliteFail[] =
{
	{
		tlWEliteFail,
		ARRAYSIZE( tlWEliteFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"WElite Fail"
	},
};

//=========================================================
// Combat Fail Schedule
//=========================================================
Task_t tlWEliteCombatFail[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2.0f },
	{ TASK_WAIT_PVS, 0.0f },
};

Schedule_t slWEliteCombatFail[] =
{
	{
		tlWEliteCombatFail,
		ARRAYSIZE( tlWEliteCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"WElite Combat Fail"
	},
};

//=========================================================
// Standoff schedule. Used in combat when a monster is 
// hiding in cover or the enemy has moved out of sight. 
// Should we look around in this schedule?
//=========================================================
Task_t tlWEliteStandoff[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2.0f },
};

Schedule_t slWEliteStandoff[] =
{
	{
		tlWEliteStandoff,
		ARRAYSIZE( tlWEliteStandoff ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_SEE_ENEMY |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"welite Standoff"
	}
};

//=========================================================
// Suppress
//=========================================================
Task_t tlWEliteSuppressHornet[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_RANGE_ATTACK1, 0.0f },
};

Schedule_t slWEliteSuppress[] =
{
	{
		tlWEliteSuppressHornet,
		ARRAYSIZE( tlWEliteSuppressHornet ),
		0,
		0,
		"WElite Suppress Hornet",
	},
};

//=========================================================
// primary range attacks
//=========================================================
Task_t tlWEliteRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_RANGE_ATTACK1, 0.0f },
};

Schedule_t slWEliteRangeAttack1[] =
{
	{
		tlWEliteRangeAttack1,
		ARRAYSIZE( tlWEliteRangeAttack1 ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_HEAVY_DAMAGE,
		0,
		"WElite Range Attack1"
	},
};

Task_t tlWEliteHiddenRangeAttack1[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_STANDOFF },
	{ TASK_WELITE_SETUP_HIDE_ATTACK, 0 },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_RANGE_ATTACK1_NOTURN, (float)0 },
};

Schedule_t slWEliteHiddenRangeAttack[] =
{
	{
		tlWEliteHiddenRangeAttack1,
		ARRAYSIZE ( tlWEliteHiddenRangeAttack1 ),
		bits_COND_NEW_ENEMY |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"WElite Hidden Range Attack1"
	},
};

//=========================================================
// Take cover from enemy! Tries lateral cover before node
// cover!
//=========================================================
Task_t tlWEliteTakeCoverFromEnemy[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_WAIT, 0.2f },
	{ TASK_FIND_COVER_FROM_ENEMY, 0.0f },
	{ TASK_RUN_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_REMEMBER, (float)bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0.0f },
};

Schedule_t slWEliteTakeCoverFromEnemy[] =
{
	{
		tlWEliteTakeCoverFromEnemy,
		ARRAYSIZE( tlWEliteTakeCoverFromEnemy ),
		bits_COND_NEW_ENEMY,
		0,
		"WEliteTakeCoverFromEnemy"
	},
};

//=========================================================
// Victory dance!
//=========================================================
Task_t tlWEliteVictoryDance[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_WELITE_THREAT_DISPLAY },
	{ TASK_WAIT, 0.2f },
	{ TASK_WELITE_GET_PATH_TO_ENEMY_CORPSE,	0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float)ACT_CROUCH },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_STAND },
	{ TASK_PLAY_SEQUENCE, (float)ACT_THREAT_DISPLAY },
	{ TASK_PLAY_SEQUENCE, (float)ACT_CROUCH },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float)ACT_STAND },
};

Schedule_t slWEliteVictoryDance[] =
{
	{
		tlWEliteVictoryDance,
		ARRAYSIZE( tlWEliteVictoryDance ),
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE,
		0,
		"WEliteVictoryDance"
	},
};

//=========================================================
//=========================================================
Task_t tlWEliteThreatDisplay[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float)ACT_THREAT_DISPLAY },
};

Schedule_t slWEliteThreatDisplay[] =
{
	{
		tlWEliteThreatDisplay,
		ARRAYSIZE( tlWEliteThreatDisplay ),
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE,
		bits_SOUND_PLAYER |
		bits_SOUND_COMBAT |
		bits_SOUND_WORLD,
		"WEliteThreatDisplay"
	},
};

DEFINE_CUSTOM_SCHEDULES( CWElite )
{
	slWEliteFail,
	slWEliteCombatFail,
	slWEliteStandoff,
	slWEliteSuppress,
	slWEliteRangeAttack1,
	slWEliteHiddenRangeAttack,
	slWEliteTakeCoverFromEnemy,
	slWEliteVictoryDance,
	slWEliteThreatDisplay,
};

IMPLEMENT_CUSTOM_SCHEDULES( CWElite, CSquadMonster )

//=========================================================
// FCanCheckAttacks - this is overridden for horde elites
// because they can use their smart weapons against unseen
// enemies. Base class doesn't attack anyone it can't see.
//=========================================================
BOOL CWElite::FCanCheckAttacks( void )
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
// CheckMeleeAttack1 - horde elites zap the crap out of 
// any enemy that gets too close. 
//=========================================================
BOOL CWElite::CheckMeleeAttack1( float flDot, float flDist )
{
	if( HasConditions( bits_COND_SEE_ENEMY ) && flDist <= WELITE_MELEE_DIST && flDot >= 0.6f && m_hEnemy != 0 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack1 
//
// !!!LATER - we may want to load balance this. Several
// tracelines are done, so we may not want to do this every
// server frame. Definitely not while firing. 
//=========================================================
BOOL CWElite::CheckRangeAttack1( float flDot, float flDist )
{
	if( gpGlobals->time < m_flNextPlasmaAttackCheck )
	{
		return m_fCanPlasmaAttack;
	}

	if( HasConditions( bits_COND_SEE_ENEMY ) && flDist >= WELITE_MELEE_DIST && flDist <= 1024.0f && flDot >= 0.5f && NoFriendlyFire() )
	{
		TraceResult tr;
		Vector	vecArmPos, vecArmDir;

		// verify that a shot fired from the gun will hit the enemy before the world.
		// !!!LATER - we may wish to do something different for projectile weapons as opposed to instant-hit
		UTIL_MakeVectors( pev->angles );
		GetAttachment( 0, vecArmPos, vecArmDir );
		//UTIL_TraceLine( vecArmPos, vecArmPos + gpGlobals->v_forward * 256.0f, ignore_monsters, ENT( pev ), &tr );
		UTIL_TraceLine( vecArmPos, m_hEnemy->BodyTarget( vecArmPos ), dont_ignore_monsters, ENT( pev ), &tr );

		if( tr.flFraction == 1.0f || tr.pHit == m_hEnemy->edict() )
		{
			m_flNextPlasmaAttackCheck = gpGlobals->time + RANDOM_FLOAT( 2.0f, 5.0f );
			m_fCanPlasmaAttack = TRUE;
			return m_fCanPlasmaAttack;
		}
	}

	m_flNextPlasmaAttackCheck = gpGlobals->time + 0.2f;// don't check for half second if this check wasn't successful
	m_fCanPlasmaAttack = FALSE;
	return m_fCanPlasmaAttack;
}

//=========================================================
// StartTask
//=========================================================
void CWElite::StartTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_WELITE_GET_PATH_TO_ENEMY_CORPSE:
		{
			UTIL_MakeVectors( pev->angles );
			if( BuildRoute( m_vecEnemyLKP - gpGlobals->v_forward * 50.0f, bits_MF_TO_LOCATION, NULL ) )
			{
				TaskComplete();
			}
			else
			{
				ALERT( at_aiconsole, "WEliteGetPathToEnemyCorpse failed!!\n" );
				TaskFail();
			}
		}
		break;
	case TASK_WELITE_SETUP_HIDE_ATTACK:
		// horde elites shoots hornets back out into the open from a concealed location. 
		// try to find a spot to throw that gives the smart weapon a good chance of finding the enemy.
		// ideally, this spot is along a line that is perpendicular to a line drawn from the welite to the enemy.
		CBaseMonster	*pEnemyMonsterPtr;

		pEnemyMonsterPtr = m_hEnemy->MyMonsterPointer();

		if( pEnemyMonsterPtr )
		{
			Vector vecCenter;
			TraceResult tr;
			BOOL fSkip;

			fSkip = FALSE;
			vecCenter = Center();

			UTIL_VecToAngles( m_vecEnemyLKP - pev->origin );

			UTIL_TraceLine( Center() + gpGlobals->v_forward * 128.0f, m_vecEnemyLKP, ignore_monsters, ENT( pev ), &tr );
			if( tr.flFraction == 1.0f )
			{
				MakeIdealYaw( pev->origin + gpGlobals->v_right * 128.0f );
				fSkip = TRUE;
				TaskComplete();
			}

			if( !fSkip )
			{
				UTIL_TraceLine( Center() - gpGlobals->v_forward * 128.0f, m_vecEnemyLKP, ignore_monsters, ENT( pev ), &tr );
				if( tr.flFraction == 1.0f )
				{
					MakeIdealYaw( pev->origin - gpGlobals->v_right * 128.0f );
					fSkip = TRUE;
					TaskComplete();
				}
			}

			if( !fSkip )
			{
				UTIL_TraceLine( Center() + gpGlobals->v_forward * 256.0f, m_vecEnemyLKP, ignore_monsters, ENT( pev ), &tr );
				if( tr.flFraction == 1.0f )
				{
					MakeIdealYaw( pev->origin + gpGlobals->v_right * 256.0f );
					fSkip = TRUE;
					TaskComplete();
				}
			}

			if( !fSkip )
			{
				UTIL_TraceLine( Center() - gpGlobals->v_forward * 256.0f, m_vecEnemyLKP, ignore_monsters, ENT( pev ), &tr );
				if( tr.flFraction == 1.0f )
				{
					MakeIdealYaw( pev->origin - gpGlobals->v_right * 256.0f );
					fSkip = TRUE;
					TaskComplete();
				}
			}

			if( !fSkip )
			{
				TaskFail();
			}
		}
		else
		{
			ALERT( at_aiconsole, "welite - no enemy monster ptr!!!\n" );
			TaskFail();
		}
		break;
	default:
		CSquadMonster::StartTask( pTask );
		break;
	}
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CWElite::GetSchedule( void )
{
	if( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if( pSound && ( pSound->m_iType & bits_SOUND_DANGER ) )
		{
			// dangerous sound nearby!
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
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

			if( HasConditions( bits_COND_NEW_ENEMY ) )
			{
				return GetScheduleOfType( SCHED_WAKE_ANGRY );
			}

			// zap player!
			if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				AttackSound();// this is a total hack. Should be parto f the schedule
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
			}

			if( HasConditions( bits_COND_HEAVY_DAMAGE ) )
			{
				return GetScheduleOfType( SCHED_SMALL_FLINCH );
			}

			// can attack
			if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot ( bits_SLOTS_WELITE_PLASMA ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}

			if( OccupySlot ( bits_SLOT_WELITE_CHASE ) )
			{
				return GetScheduleOfType( SCHED_CHASE_ENEMY );
			}

			return GetScheduleOfType( SCHED_STANDOFF );
		}
		break;
	default:
		break;
	}

	return CSquadMonster::GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t *CWElite::GetScheduleOfType( int Type ) 
{
	switch( Type )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return &slWEliteTakeCoverFromEnemy[0];
		break;
	case SCHED_RANGE_ATTACK1:
		if( HasConditions( bits_COND_SEE_ENEMY ) )
		{
			//normal attack
			return &slWEliteRangeAttack1[0];
		}
		else
		{
			// attack an unseen enemy
			// return &slWEliteHiddenRangeAttack[0];
			return &slWEliteRangeAttack1[0];
		}
		break;
	case SCHED_WELITE_THREAT_DISPLAY:
		return &slWEliteThreatDisplay[0];
		break;
	case SCHED_WELITE_SUPPRESS:
		return &slWEliteSuppress[0];
		break;
	case SCHED_STANDOFF:
		return &slWEliteStandoff[0];
		break;
	case SCHED_VICTORY_DANCE:
		return &slWEliteVictoryDance[0];
		break;
	case SCHED_FAIL:
		// no fail schedule specified, so pick a good generic one.
		{
			if( m_hEnemy != 0 )
			{
				// I have an enemy
				// !!!LATER - what if this enemy is really far away and i'm chasing him?
				// this schedule will make me stop, face his last known position for 2 
				// seconds, and then try to move again
				return &slWEliteCombatFail[0];
			}

			return &slWEliteFail[0];
		}
		break;
	}

	return CSquadMonster::GetScheduleOfType( Type );
}


void CWElite::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( pev->spawnflags & SF_MONSTER_NO_WPN_DROP )
	{// drop the hornetgun!
		Vector vecGunPos;
		Vector vecGunAngles;

		pev->body = WELITE_BODY_NOGUN;

		GetAttachment( 0, vecGunPos, vecGunAngles );
		
		DropItem( "weapon_hornetgun", vecGunPos, vecGunAngles );
	}

	CBaseMonster::Killed( pevAttacker, iGib );
}

class CWElitePlasma : public CBaseMonster
{
	void Spawn( void );
	void Precache( void );
	void EXPORT AnimateThink( void );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );

	EHANDLE m_hOwner;
};

LINK_ENTITY_TO_CLASS( welite_plasma, CWElitePlasma )

void CWElitePlasma::Spawn( void )
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "sprites/xspark4.spr" );
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->scale = 0.5f;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( this, pev->origin );

	SetThink( &CWElitePlasma::AnimateThink );
	SetTouch( &CWElitePlasma::ExplodeTouch );

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	SetNextThink( 0.1f );
}

void CWElitePlasma::Precache( void )
{
	PRECACHE_MODEL( "sprites/xspark4.spr" );
	// PRECACHE_SOUND( "debris/zap4.wav" );
	// PRECACHE_SOUND( "weapons/electro4.wav" );
}

void CWElitePlasma::AnimateThink( void )
{
	SetNextThink( 0.1f );

	pev->frame = ( (int)pev->frame + 1 ) % 11;

	if( gpGlobals->time - pev->dmgtime > 5 || pev->velocity.Length() < 10.0f )
	{
		SetTouch( NULL );
		UTIL_Remove( this );
	}
}

void CWElitePlasma::ExplodeTouch( CBaseEntity *pOther )
{
	if( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace();

		entvars_t *pevOwner;

		if( m_hOwner != 0 )
		{
			pevOwner = m_hOwner->pev;
		}
		else
		{
			pevOwner = pev;
		}

		ClearMultiDamage();
		pOther->TraceAttack( pevOwner, gSkillData.weliteDmgBall, pev->velocity.Normalize(), &tr, DMG_ENERGYBEAM ); 
		ApplyMultiDamage( pevOwner, pevOwner );

		UTIL_EmitAmbientSound( ENT( pev ), tr.vecEndPos, "weapons/electro4.wav", 0.3f, ATTN_NORM, 0, RANDOM_LONG( 90, 99 ) );
	}

	UTIL_Remove( this );
}
