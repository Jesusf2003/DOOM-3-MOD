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

#include "sys/platform.h"
#include "renderer/Material.h"
#include "gamesys/SysCvar.h"
#include "ai/AI.h"
#include "Player.h"

#include "Game_local.h"
#include "SoundProp.h"

// objects settling right after a map loads aren't noises anybody made
const int	SOUNDPROP_SETTLE_MSEC			= 2000;
const int	SOUNDPROP_DEBUG_MSEC			= 2000;		// how long the debug drawing stays up

static const char *soundTypeNames[ SND_TYPE_COUNT ] = {
	"footstep",
	"impact",
	"combat",
	"distraction"
};

/*
================
SoundProp_TypeName
================
*/
const char *SoundProp_TypeName( soundType_t type ) {
	if ( type < 0 || type >= SND_TYPE_COUNT ) {
		return "unknown";
	}
	return soundTypeNames[ type ];
}

/*
================
SoundProp_SurfaceModifier

Hard and resonant surfaces carry the noise further, soft ones muffle it.
================
*/
float SoundProp_SurfaceModifier( int surfaceType ) {
	switch( surfaceType ) {
		case SURFTYPE_METAL:		return 8.0f;
		case SURFTYPE_RICOCHET:		return 8.0f;
		case SURFTYPE_GLASS:		return 6.0f;
		case SURFTYPE_LIQUID:		return 6.0f;
		case SURFTYPE_WOOD:			return 4.0f;
		case SURFTYPE_STONE:		return 2.0f;
		case SURFTYPE_PLASTIC:		return 0.0f;
		case SURFTYPE_CARDBOARD:	return -4.0f;
		case SURFTYPE_FLESH:		return -6.0f;
		default:					return 0.0f;
	}
}

/*
================
SoundProp_DebugAxis

Debug text faces the local player.
================
*/
static idMat3 SoundProp_DebugAxis( void ) {
	idPlayer *player = gameLocal.GetLocalPlayer();
	return player ? player->viewAngles.ToMat3() : mat3_identity;
}

/*
================
idGameLocal::GetSoundObstacleLoss

dB lost on the way from the source to the listener. Like TDM, a closed portal (a shut door) between
the two areas is the biggest loss; with an open path, a wall on the straight line still muffles the
sound that has to go around it. Actors don't block sound (CONTENTS_BODY isn't traced).
================
*/
float idGameLocal::GetSoundObstacleLoss( const idVec3 &from, const idVec3 &to, const idEntity *maker, soundObstacle_t &obstacle ) const {
	const int areaFrom = gameRenderWorld->PointInArea( from );
	const int areaTo = gameRenderWorld->PointInArea( to );
	if ( areaFrom >= 0 && areaTo >= 0 && areaFrom != areaTo && !gameRenderWorld->AreasAreConnected( areaFrom, areaTo, PS_BLOCK_VIEW ) ) {
		obstacle = SOUND_OBSTACLE_PORTAL;
		return g_soundPropPortalLoss.GetFloat();
	}

	trace_t tr;
	gameLocal.clip.TracePoint( tr, from, to, CONTENTS_SOLID, maker );
	if ( tr.fraction < 1.0f ) {
		obstacle = SOUND_OBSTACLE_WALL;
		return g_soundPropWallLoss.GetFloat();
	}

	obstacle = SOUND_OBSTACLE_NONE;
	return 0.0f;
}

/*
================
idGameLocal::EmitSoundEvent

Propagates a gameplay noise to every idAI in range. Server side only, the AI runs there.
================
*/
void idGameLocal::EmitSoundEvent( const soundEvent_t &event ) {
	if ( !g_soundProp.GetBool() || isClient || event.volume < SOUNDPROP_MIN_VOLUME ) {
		return;
	}
	if ( time < SOUNDPROP_SETTLE_MSEC && event.type == SND_TYPE_DISTRACTION ) {
		return;
	}

	const float rate = Max( g_soundPropAttenuation.GetFloat(), 0.001f );
	// past this nothing is left of it, even in the open
	const float maxDistance = Min( event.volume / rate, SOUNDPROP_MAX_DISTANCE );
	const int debug = g_debugSoundProp.GetInteger();
	const idMat3 debugAxis = debug ? SoundProp_DebugAxis() : mat3_identity;

	if ( debug ) {
		gameRenderWorld->DebugCircle( colorCyan, event.origin, idVec3( 0.0f, 0.0f, 1.0f ), maxDistance, 32, SOUNDPROP_DEBUG_MSEC );
		gameRenderWorld->DrawText( va( "%s %.0f dB", SoundProp_TypeName( event.type ), event.volume ), event.origin + idVec3( 0.0f, 0.0f, 8.0f ),
			0.15f, colorCyan, debugAxis, 1, SOUNDPROP_DEBUG_MSEC );
	}

	const idActor *makerActor = ( event.maker && event.maker->IsType( idActor::Type ) ) ? static_cast<const idActor *>( event.maker ) : NULL;

	for ( idEntity *ent = spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( ent == event.maker || !ent->IsType( idAI::Type ) ) {
			continue;
		}

		idAI *listener = static_cast<idAI *>( ent );
		if ( !listener->CanHearSounds() ) {
			continue;
		}
		// own team's noises are business as usual
		if ( makerActor && makerActor->team == listener->team ) {
			continue;
		}

		const idVec3 ear = listener->GetEyePosition();
		const float distance = ( ear - event.origin ).Length();
		if ( distance > maxDistance ) {
			continue;
		}

		soundObstacle_t obstacle;
		const float loss = GetSoundObstacleLoss( event.origin, ear, event.maker, obstacle );
		const float received = event.volume - distance * rate - loss;

		if ( debug >= 2 ) {
			const idVec4 &color = ( obstacle == SOUND_OBSTACLE_PORTAL ) ? colorRed : ( ( obstacle == SOUND_OBSTACLE_WALL ) ? colorYellow : colorWhite );
			gameRenderWorld->DebugLine( color, event.origin, ear, SOUNDPROP_DEBUG_MSEC );
			gameRenderWorld->DrawText( va( "%.1f dB", received ), ( event.origin + ear ) * 0.5f, 0.12f, color, debugAxis, 1, SOUNDPROP_DEBUG_MSEC );
		}

		listener->OnHeardSound( event.origin, received, event.type );
	}
}

/*
================
idGameLocal::EmitSoundEvent
================
*/
void idGameLocal::EmitSoundEvent( const idVec3 &origin, float volume, soundType_t type, idEntity *maker ) {
	soundEvent_t event;
	event.origin = origin;
	event.volume = volume;
	event.type = type;
	event.maker = maker;
	EmitSoundEvent( event );
}
