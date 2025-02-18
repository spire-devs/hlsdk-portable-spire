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

/* all this mess was here to use quake mathlib instead of hlsdk vectors
* it may break debug info or even build because global symbols types differ
*  it's better to define VectorCopy macro for Vector class */
#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int BOOL;
#define TRUE	1	
#define FALSE	0

// hack into header files that we can ship
typedef int qboolean;
typedef unsigned char byte;
#include "mathlib.h"
#include "const.h"
#include "progdefs.h"
#include "edict.h"
#include "eiface.h"

#include "studio.h"

#if !defined(ACTIVITY_H)
#include "activity.h"
#endif

#include "activitymap.h"

#if !defined(ANIMATION_H)
#include "animation.h"
#endif

#if !defined(SCRIPTEVENT_H)
#include "scriptevent.h"
#endif

#if !defined(ENGINECALLBACK_H)
#include "enginecallback.h"
#endif

//extern globalvars_t				*gpGlobals;
#else
#include "extdll.h"
#include "util.h"
#include "activity.h"
#include "activitymap.h"
#include "animation.h"
#include "scriptevent.h"
#include "studio.h"
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
#endif

#pragma warning( disable : 4244 )

int ExtractBbox( void *pmodel, int sequence, float *mins, float *maxs )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return 0;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex );

	mins[0] = pseqdesc[sequence].bbmin[0];
	mins[1] = pseqdesc[sequence].bbmin[1];
	mins[2] = pseqdesc[sequence].bbmin[2];

	maxs[0] = pseqdesc[sequence].bbmax[0];
	maxs[1] = pseqdesc[sequence].bbmax[1];
	maxs[2] = pseqdesc[sequence].bbmax[2];

	return 1;
}

int LookupActivity( void *pmodel, entvars_t *pev, int activity )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return 0;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex );

	int weighttotal = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].activity == activity )
		{
			weighttotal += pseqdesc[i].actweight;
			if( !weighttotal || RANDOM_LONG( 0, weighttotal - 1 ) < pseqdesc[i].actweight )
				seq = i;
		}
	}

	return seq;
}

int LookupActivityHeaviest( void *pmodel, entvars_t *pev, int activity )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return 0;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex );

	int weight = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].activity == activity )
		{
			if( pseqdesc[i].actweight > weight )
			{
				weight = pseqdesc[i].actweight;
				seq = i;
			}
		}
	}

	return seq;
}

void GetEyePosition( void *pmodel, float *vecEyePosition )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;

	if( !pstudiohdr )
	{
		ALERT( at_console, "GetEyePosition() Can't get pstudiohdr ptr!\n" );
		return;
	}

	VectorCopy( pstudiohdr->eyeposition, vecEyePosition );
}

int LookupSequence( void *pmodel, const char *label )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return 0;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex );

	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( stricmp( pseqdesc[i].label, label ) == 0 )
			return i;
	}

	return -1;
}

int IsSoundEvent( int eventNumber )
{
	if( eventNumber == SCRIPT_EVENT_SOUND || eventNumber == SCRIPT_EVENT_SOUND_VOICE )
		return 1;
	return 0;
}

void SequencePrecache( void *pmodel, const char *pSequenceName )
{
	int index = LookupSequence( pmodel, pSequenceName );
	if( index >= 0 )
	{
		studiohdr_t *pstudiohdr;

		pstudiohdr = (studiohdr_t *)pmodel;
		if( !pstudiohdr || index >= pstudiohdr->numseq )
			return;

		mstudioseqdesc_t *pseqdesc;
		mstudioevent_t *pevent;

		pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + index;
		pevent = (mstudioevent_t *)( (byte *)pstudiohdr + pseqdesc->eventindex );

		for( int i = 0; i < pseqdesc->numevents; i++ )
		{
			// Don't send client-side events to the server AI
			if( pevent[i].event >= EVENT_CLIENT )
				continue;

			// UNDONE: Add a callback to check to see if a sound is precached yet and don't allocate a copy
			// of it's name if it is.
			if( IsSoundEvent( pevent[i].event ) )
			{
				if( pevent[i].options[0] == '\0' )
				{
					ALERT( at_error, "Bad sound event %d in sequence %s :: %s (sound is \"%s\")\n", pevent[i].event, pstudiohdr->name, pSequenceName, pevent[i].options );
				}

				PRECACHE_SOUND( gpGlobals->pStringBase + ALLOC_STRING( pevent[i].options ) );
			}
		}
	}
}

void GetSequenceInfo( void *pmodel, entvars_t *pev, float *pflFrameRate, float *pflGroundSpeed )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return;

	mstudioseqdesc_t *pseqdesc;

	if( pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq )
	{
		*pflFrameRate = 0.0f;
		*pflGroundSpeed = 0.0f;
		return;
	}

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + (int)pev->sequence;

	if( pseqdesc->numframes > 1 )
	{
		*pflFrameRate = 256.0f * pseqdesc->fps / ( pseqdesc->numframes - 1 );
		*pflGroundSpeed = sqrt( pseqdesc->linearmovement[0] * pseqdesc->linearmovement[0] + pseqdesc->linearmovement[1] * pseqdesc->linearmovement[1] + pseqdesc->linearmovement[2] * pseqdesc->linearmovement[2] );
		*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / ( pseqdesc->numframes - 1 );
	}
	else
	{
		*pflFrameRate = 256.0f;
		*pflGroundSpeed = 0.0f;
	}
}

int GetSequenceFlags( void *pmodel, entvars_t *pev )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr || pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq )
		return 0;

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + (int)pev->sequence;

	return pseqdesc->flags;
}

int GetAnimationEvent( void *pmodel, entvars_t *pev, MonsterEvent_t *pMonsterEvent, float flStart, float flEnd, int index )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr || pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq || !pMonsterEvent )
		return 0;

	mstudioseqdesc_t *pseqdesc;
	mstudioevent_t *pevent;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + (int)pev->sequence;
	pevent = (mstudioevent_t *)( (byte *)pstudiohdr + pseqdesc->eventindex );

	if( pseqdesc->numevents == 0 || index > pseqdesc->numevents )
		return 0;

	if( pseqdesc->numframes > 1 )
	{
		flStart *= ( pseqdesc->numframes - 1 ) / 256.0f;
		flEnd *= (pseqdesc->numframes - 1) / 256.0f;
	}
	else
	{
		flStart = 0.0f;
		flEnd = 1.0f;
	}

	for( ; index < pseqdesc->numevents; index++ )
	{
		// Don't send client-side events to the server AI
		if( pevent[index].event >= EVENT_CLIENT )
			continue;

		if( ( pevent[index].frame >= flStart && pevent[index].frame < flEnd ) ||
			( ( pseqdesc->flags & STUDIO_LOOPING ) && flEnd >= pseqdesc->numframes - 1 && pevent[index].frame < flEnd - pseqdesc->numframes + 1 ) )
		{
			pMonsterEvent->event = pevent[index].event;
			pMonsterEvent->options = pevent[index].options;
			return index + 1;
		}
	}
	return 0;
}

float SetController( void *pmodel, entvars_t *pev, int iController, float flValue )
{
	studiohdr_t *pstudiohdr;
	int i;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return flValue;

	mstudiobonecontroller_t	*pbonecontroller = (mstudiobonecontroller_t *)( (byte *)pstudiohdr + pstudiohdr->bonecontrollerindex );

	// find first controller that matches the index
	for( i = 0; i < pstudiohdr->numbonecontrollers; i++, pbonecontroller++ )
	{
		if( pbonecontroller->index == iController )
			break;
	}
	if( i >= pstudiohdr->numbonecontrollers )
		return flValue;

	// wrap 0..360 if it's a rotational controller
	if( pbonecontroller->type & ( STUDIO_XR | STUDIO_YR | STUDIO_ZR ) )
	{
		// ugly hack, invert value if end < start
		if( pbonecontroller->end < pbonecontroller->start )
			flValue = -flValue;

		// does the controller not wrap?
		if( pbonecontroller->start + 359.0f >= pbonecontroller->end )
		{
			if( flValue > ( ( pbonecontroller->start + pbonecontroller->end ) * 0.5f ) + 180.0f )
				flValue = flValue - 360.0f;
			if( flValue < ( ( pbonecontroller->start + pbonecontroller->end ) * 0.5f ) - 180.0f )
				flValue = flValue + 360.0f;
		}
		else
		{
			if( flValue > 360.0f )
				flValue = flValue - (int)( flValue / 360.0f ) * 360.0f;
			else if( flValue < 0.0f )
				flValue = flValue + (int)( ( flValue / -360.0f ) + 1.0f ) * 360.0f;
		}
	}

	int setting = (int)( 255 * ( flValue - pbonecontroller->start ) / ( pbonecontroller->end - pbonecontroller->start ) );

	if( setting < 0 )
		setting = 0;
	if( setting > 255 )
		setting = 255;
	pev->controller[iController] = setting;

	return setting * ( 1.0f / 255.0f ) * (pbonecontroller->end - pbonecontroller->start ) + pbonecontroller->start;
}

float SetBlending( void *pmodel, entvars_t *pev, int iBlender, float flValue )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr || pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq )
		return flValue;

	mstudioseqdesc_t *pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + (int)pev->sequence;

	if( pseqdesc->blendtype[iBlender] == 0 )
		return flValue;

	if( pseqdesc->blendtype[iBlender] & ( STUDIO_XR | STUDIO_YR | STUDIO_ZR ) )
	{
		// ugly hack, invert value if end < start
		if( pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender] )
			flValue = -flValue;

		// does the controller not wrap?
		if( pseqdesc->blendstart[iBlender] + 359.0f >= pseqdesc->blendend[iBlender] )
		{
			if( flValue > ( ( pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender] ) * 0.5f ) + 180.0f )
				flValue = flValue - 360.0f;
			if( flValue < ( ( pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender] ) * 0.5f ) - 180.0f )
				flValue = flValue + 360.0f;
		}
	}

	int setting = (int)( 255 * ( flValue - pseqdesc->blendstart[iBlender] ) / ( pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender] ) );

	if( setting < 0 )
		setting = 0;
	if(setting > 255)
		setting = 255;

	pev->blending[iBlender] = setting;

	return setting * ( 1.0f / 255.0f ) * ( pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender] ) + pseqdesc->blendstart[iBlender];
}

int FindTransition( void *pmodel, int iEndingAnim, int iGoalAnim, int *piDir )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return iGoalAnim;

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex );

	// bail if we're going to or from a node 0
	if( pseqdesc[iEndingAnim].entrynode == 0 || pseqdesc[iGoalAnim].entrynode == 0 )
	{
		return iGoalAnim;
	}

	int iEndNode;

	// ALERT( at_console, "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	if( *piDir > 0 )
	{
		iEndNode = pseqdesc[iEndingAnim].exitnode;
	}
	else
	{
		iEndNode = pseqdesc[iEndingAnim].entrynode;
	}

	if( iEndNode == pseqdesc[iGoalAnim].entrynode )
	{
		*piDir = 1;
		return iGoalAnim;
	}

	byte *pTransition = ( (byte *)pstudiohdr + pstudiohdr->transitionindex );

	int iInternNode = pTransition[( iEndNode - 1 ) * pstudiohdr->numtransitions + ( pseqdesc[iGoalAnim].entrynode - 1 )];

	if( iInternNode == 0 )
		return iGoalAnim;

	int i;

	// look for someone going
	for( i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].entrynode == iEndNode && pseqdesc[i].exitnode == iInternNode )
		{
			*piDir = 1;
			return i;
		}
		if( pseqdesc[i].nodeflags )
		{
			if( pseqdesc[i].exitnode == iEndNode && pseqdesc[i].entrynode == iInternNode )
			{
				*piDir = -1;
				return i;
			}
		}
	}

	ALERT( at_console, "error in transition graph" );
	return iGoalAnim;
}

void SetBodygroup( void *pmodel, entvars_t *pev, int iGroup, int iValue )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return;

	if( iGroup > pstudiohdr->numbodyparts )
		return;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)( (byte *)pstudiohdr + pstudiohdr->bodypartindex ) + iGroup;

	if( iValue >= pbodypart->nummodels )
		return;

	int iCurrent = ( pev->body / pbodypart->base ) % pbodypart->nummodels;

	pev->body = ( pev->body - ( iCurrent * pbodypart->base ) + ( iValue * pbodypart->base ) );
}

int GetBodygroup( void *pmodel, entvars_t *pev, int iGroup )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if( !pstudiohdr )
		return 0;

	if( iGroup > pstudiohdr->numbodyparts )
		return 0;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)( (byte *)pstudiohdr + pstudiohdr->bodypartindex ) + iGroup;

	if( pbodypart->nummodels <= 1 )
		return 0;

	int iCurrent = ( pev->body / pbodypart->base ) % pbodypart->nummodels;

	return iCurrent;
}

//LRC
int GetBoneCount( void *pmodel )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (!pstudiohdr)
	{
		ALERT(at_error, "Bad header in SetBones!\n");
		return 0;
	}

	return pstudiohdr->numbones;
}

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

//LRC
void SetBones( void *pmodel, float (*data)[3], int datasize)
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (!pstudiohdr)
	{
		ALERT(at_error, "Bad header in SetBones!\n");
		return;
	}

	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)pstudiohdr + pstudiohdr->boneindex);

//	ALERT(at_console, "List begins:\n");
	int j;
	int limit = min(pstudiohdr->numbones, datasize);
	// go through the bones
	for (int i = 0; i < limit; i++, pbone++)
	{
//		ALERT(at_console, " %s\n", pbone->name);
		for (j = 0; j < 3; j++)
			pbone->value[j] = data[i][j];
	}
//	ALERT(at_console, "List ends.\n");
}
