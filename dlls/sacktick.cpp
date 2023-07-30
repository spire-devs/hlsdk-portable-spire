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
// sacktick.cpp - tiny, jumpy alien parasite
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"game.h"

#define 	ST_ATTN_IDLE 		(float)1.5

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		ST_AE_JUMPATTACK	( 2 )

Task_t tlSTRangeAttack1[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_IDEAL, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_FACE_IDEAL, (float)0 },
	{ TASK_WAIT_RANDOM, (float)0.5 },
};

Schedule_t slSTRangeAttack1[] =
{
	{
		tlSTRangeAttack1,
		ARRAYSIZE( tlSTRangeAttack1 ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NO_AMMO_LOADED,
		0,
		"STRangeAttack1"
	},
};

Task_t tlSTRangeAttack2[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_IDEAL, (float)0 },
	{ TASK_RANGE_ATTACK1, (float)0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
};

Schedule_t slSTRangeAttack2[] =
{
	{
		tlSTRangeAttack2,
		ARRAYSIZE( tlSTRangeAttack2 ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NO_AMMO_LOADED,
		0,
		"STRAFast"
	},
};

class CSacktick : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void RunTask ( Task_t *pTask );
	void StartTask ( Task_t *pTask );
	void SetYawSpeed ( void );
	void EXPORT LeapTouch ( CBaseEntity *pOther );
	Vector Center( void );
	Vector BodyTarget( const Vector &posSrc );
	void PainSound( void );
	void DeathSound( void );
	void IdleSound( void );
	void AlertSound( void );
	void PrescheduleThink( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	virtual float GetDamageAmount( void ) { return gSkillData.headcrabDmgBite * 0.5; }
	virtual int GetVoicePitch( void ) { return 100; }
	virtual float GetSoundVolue( void ) { return 1.0; }
	Schedule_t* GetScheduleOfType ( int Type );

	CUSTOM_SCHEDULES

	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pDeathSounds[];
	static const char *pBiteSounds[];
};

LINK_ENTITY_TO_CLASS( monster_sacktick, CSacktick )

DEFINE_CUSTOM_SCHEDULES( CSacktick )
{
	slSTRangeAttack1,
	slSTRangeAttack2,
};

IMPLEMENT_CUSTOM_SCHEDULES( CSacktick, CBaseMonster )

const char *CSacktick::pIdleSounds[] =
{
	"sacktick/st_idle1.wav",
	"sacktick/st_idle2.wav",
	"sacktick/st_idle3.wav",
	"sacktick/st_idle4.wav",
	"sacktick/st_idle5.wav",
};

const char *CSacktick::pAlertSounds[] =
{
	"sacktick/st_alert1.wav",
};

const char *CSacktick::pPainSounds[] =
{
	"sacktick/st_pain1.wav",
	"sacktick/st_pain2.wav",
	"sacktick/st_pain3.wav",
};

const char *CSacktick::pAttackSounds[] =
{
	"sacktick/st_attack1.wav",
	"sacktick/st_attack2.wav",
	"sacktick/st_attack3.wav",
};

const char *CSacktick::pDeathSounds[] =
{
	"sacktick/st_die1.wav",
	"sacktick/st_die2.wav",
};

const char *CSacktick::pBiteSounds[] =
{
	"sacktick/st_headbite.wav",
};

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int CSacktick::Classify( void )
{
	return m_iClass?m_iClass:CLASS_WARRIOR_BIOWEAPON;
}

//=========================================================
// Center - returns the real center of the sacktick.  The 
// bounding box is much larger than the actual creature so 
// this is needed for targeting
//=========================================================
Vector CSacktick::Center( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 6.0f );
}

Vector CSacktick::BodyTarget( const Vector &posSrc ) 
{ 
	return Center();
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSacktick::SetYawSpeed( void )
{
	int ys;

	switch( m_Activity )
	{
	case ACT_IDLE:	
		ys = 30;
		break;
	case ACT_RUN:
	case ACT_WALK:
		ys = 20;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 60;
		break;
	case ACT_RANGE_ATTACK1:
		ys = 30;
		break;
	default:
		ys = 30;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CSacktick::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case ST_AE_JUMPATTACK:
		{
			ClearBits( pev->flags, FL_ONGROUND );

			UTIL_SetOrigin (this, pev->origin + Vector ( 0 , 0 , 1) );// take him off ground so engine doesn't instantly reset onground 
			UTIL_MakeVectors( pev->angles );

			Vector vecJumpDir;
			if( m_hEnemy != 0 )
			{
				float gravity = g_psv_gravity->value;
				if( gravity <= 1 )
					gravity = 1;

				// How fast does the sacktick need to travel to reach that height given gravity?
				float height = m_hEnemy->pev->origin.z + m_hEnemy->pev->view_ofs.z - pev->origin.z;
				if( height < 16 )
					height = 16;
				float speed = sqrt( 2 * gravity * height );
				float time = speed / gravity;

				// Scale the sideways velocity to get there at the right time
				vecJumpDir = m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - pev->origin;
				vecJumpDir = vecJumpDir * ( 1.0f / time );

				// Speed to offset gravity at the desired height
				vecJumpDir.z = speed;

				// Don't jump too far/fast
				float distance = vecJumpDir.Length();

				if( distance > 650.0f )
				{
					vecJumpDir = vecJumpDir * ( 650.0f / distance );
				}
			}
			else
			{
				// jump hop, don't care where
				vecJumpDir = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 350.0f;
			}

			int iSound = RANDOM_LONG( 0, 2 );
			if( iSound != 0 )
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAttackSounds[iSound], GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );

			pev->velocity = vecJumpDir;
			m_flNextAttack = gpGlobals->time + 2.0f;
		}
		break;
		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CSacktick::Spawn()
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL( ENT( pev ), "models/headcrab.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if (pev->health == 0)
		pev->health		= gSkillData.headcrabHealth;
	pev->view_ofs		= Vector( 0, 0, 20 );// position of the eyes relative to monster's origin.
	pev->yaw_speed		= 5;//!!! should we put this in the monster's changeanim function since turn rates may vary with state/anim?
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSacktick::Precache()
{
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );
	PRECACHE_SOUND_ARRAY( pBiteSounds );

	if (pev->model)
		PRECACHE_MODEL(STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL( "models/headcrab.mdl" );
}

//=========================================================
// RunTask 
//=========================================================
void CSacktick::RunTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
		{
			if( m_fSequenceFinished )
			{
				TaskComplete();
				m_IdealActivity = ACT_IDLE;
			}
			break;
		}
	default:
		{
			CBaseMonster::RunTask( pTask );
		}
	}
}

//=========================================================
// LeapTouch - this is the sacktick's touch function when it
// is in the air
//=========================================================
void CSacktick::LeapTouch( CBaseEntity *pOther )
{
	if( !pOther->pev->takedamage )
	{
		return;
	}

	if( pOther->Classify() == Classify() )
	{
		return;
	}

	// Don't hit if back on ground
	if( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pBiteSounds ), GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );

		pOther->TakeDamage( pev, pev, GetDamageAmount(), DMG_SLASH );
	}
	else
	{
		SetTouch( NULL );	
	}
}

//=========================================================
// PrescheduleThink
//=========================================================
void CSacktick::PrescheduleThink( void )
{
	// make the crab coo a little bit in combat state
	if( m_MonsterState == MONSTERSTATE_COMBAT && RANDOM_FLOAT( 0, 5 ) < 0.1f )
	{
		IdleSound();
	}
}

void CSacktick::StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pAttackSounds[0], GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );
			m_IdealActivity = ACT_RANGE_ATTACK1;
			SetTouch( &CSacktick::LeapTouch );
			break;
		}
	case TASK_RANGE_ATTACK2:
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pAttackSounds[0], GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() * 1.2 );
			m_IdealActivity = ACT_RANGE_ATTACK1;
			SetTouch( &CSacktick::LeapTouch );
			break;
		}
	default:
		{
			CBaseMonster::StartTask( pTask );
		}
	}
}

//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CSacktick::CheckRangeAttack1( float flDot, float flDist )
{
	if( FBitSet( pev->flags, FL_ONGROUND ) && flDist > 64 && flDist <= 256 && flDot >= 0.65f )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack2
//=========================================================
BOOL CSacktick::CheckRangeAttack2( float flDot, float flDist )
{
	if( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 64 && flDot >= 0.5f )
	{
		return TRUE;
	}
	return FALSE;
}

int CSacktick::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	/*
	// Don't take any acid damage -- BigMomma's mortar is acid
	// Well we aren't a Headcrab so no DO take Acid Damage
	if( bitsDamageType & DMG_ACID )
		flDamage = 0;
	*/

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// IdleSound
//=========================================================
void CSacktick::IdleSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY( pIdleSounds ), GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );
}

//=========================================================
// AlertSound 
//=========================================================
void CSacktick::AlertSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );
}

//=========================================================
// AlertSound 
//=========================================================
void CSacktick::PainSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );
}

//=========================================================
// DeathSound 
//=========================================================
void CSacktick::DeathSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY( pDeathSounds ), GetSoundVolue(), ST_ATTN_IDLE, 0, GetVoicePitch() );
}

Schedule_t *CSacktick::GetScheduleOfType( int Type )
{
	switch( Type )
	{
		case SCHED_RANGE_ATTACK1:
		{
			return &slSTRangeAttack1[0];
		}
		break;
		case SCHED_RANGE_ATTACK2:
		{
			return &slSTRangeAttack2[0];
		}
		break;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}