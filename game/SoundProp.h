/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __GAME_SOUNDPROP_H__
#define __GAME_SOUNDPROP_H__

#include "idlib/math/Vector.h"

/*
===============================================================================

	Stealth sound propagation (after The Dark Mod's sound prop)

	Gameplay noises are separate from the sounds the player hears: an action posts a sound event with
	a loudness in dB, idGameLocal::EmitSoundEvent works out what reaches each idAI (distance, walls,
	closed portals) and calls idAI::OnHeardSound on those above their hearing threshold.

	received = volume - distance * g_soundPropAttenuation - wall / closed portal losses

===============================================================================
*/

class idEntity;

typedef enum {
	SND_TYPE_FOOTSTEP,
	SND_TYPE_IMPACT,		// landings, melee strikes on the world
	SND_TYPE_COMBAT,		// weapon fire
	SND_TYPE_DISTRACTION,	// thrown / knocked over objects
	SND_TYPE_COUNT
} soundType_t;

typedef enum {
	SOUND_OBSTACLE_NONE,	// straight line in the open
	SOUND_OBSTACLE_WALL,	// open path, but a wall on the straight line (g_soundPropWallLoss)
	SOUND_OBSTACLE_PORTAL	// a closed portal / door between the areas (g_soundPropPortalLoss)
} soundObstacle_t;

typedef struct soundEvent_s {
	idVec3					origin;
	float					volume;		// dB at the source
	soundType_t				type;
	idEntity *				maker;		// who made it, never hears itself (can be NULL)
} soundEvent_t;

// source loudness in dB, tuned against the default AI hearing threshold
const float	SOUNDPROP_VOLUME_CROUCH			= 30.0f;
const float	SOUNDPROP_VOLUME_WALK			= 45.0f;
const float	SOUNDPROP_VOLUME_RUN			= 55.0f;
const float	SOUNDPROP_VOLUME_LAND_SOFT		= 50.0f;	// smallest landing that counts
const float	SOUNDPROP_VOLUME_LAND_HARD		= 72.0f;	// a fall that would hurt
const float	SOUNDPROP_VOLUME_MELEE_WORLD	= 60.0f;
const float	SOUNDPROP_VOLUME_WEAPON_FIRE	= 90.0f;
const float	SOUNDPROP_VOLUME_IMPACT_MIN		= 40.0f;	// slowest object bounce that makes a sound
const float	SOUNDPROP_VOLUME_IMPACT_MAX		= 70.0f;	// fastest

const float	SOUNDPROP_DEFAULT_HEARING		= 20.0f;	// idAI "hearing_threshold" when the def has none
const float	SOUNDPROP_MAX_DISTANCE			= 2048.0f;	// hard cap of the search radius (same as AI_HEARING_RANGE)
const float	SOUNDPROP_MIN_VOLUME			= 1.0f;		// events quieter than this aren't propagated at all

const char *	SoundProp_TypeName( soundType_t type );
float			SoundProp_SurfaceModifier( int surfaceType );	// dB added for the material hit / walked on (surfTypes_t)

#endif /* !__GAME_SOUNDPROP_H__ */
