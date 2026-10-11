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
#include "gamesys/SysCvar.h"
#include "ai/AI.h"
#include "Player.h"

#include "Game_local.h"
#include "PlayerCamera.h"

const float CAM_COLLISION_SIZE		= 6.0f;		// half size of the box traced for the camera, keeps the near plane out of walls
const float CAM_SNAP_DISTANCE		= 128.0f;	// the eyes jumped further than this from the pivot (teleport): snap, don't ease
const float CAM_LOCK_PITCH_MIN		= -10.0f;	// pitch range while framing a lock-on target (positive looks down)
const float CAM_LOCK_PITCH_MAX		= 35.0f;
const float CAM_LOCK_PITCH_BIAS		= 10.0f;	// look a bit down on the target so the player stays in frame

/*
================
CamExpBlend

Fraction to move towards a goal this frame for an exponential approach at 'speed' (1/s),
the same feel at any frame rate.
================
*/
static float CamExpBlend( float speed, float dt ) {
	return 1.0f - idMath::Exp( -speed * dt );
}

/*
================
CamSmoothDamp

Critically damped spring towards target (Game Programming Gems 4, 1.10): no overshoot, and
the velocity carries over between frames so a moving target is followed without stutter.
================
*/
static void CamSmoothDamp( idVec3 &current, idVec3 &velocity, const idVec3 &target, float smoothTime, float dt ) {
	const float omega = 2.0f / Max( smoothTime, 0.0001f );
	const float x = omega * dt;
	const float decay = 1.0f / ( 1.0f + x + 0.48f * x * x + 0.235f * x * x * x );
	const idVec3 change = current - target;
	const idVec3 temp = ( velocity + change * omega ) * dt;

	velocity = ( velocity - temp * omega ) * decay;
	current = target + ( change + temp ) * decay;
}

/*
===============================================================================

	idLockOnSystem

===============================================================================
*/

/*
================
idLockOnSystem::idLockOnSystem
================
*/
idLockOnSystem::idLockOnSystem( void ) {
	maxDistance			= 1024.0f;
	maxAngle			= 60.0f;
	angleWeight			= 1.0f;
	distanceWeight		= 0.5f;
	keepDistanceScale	= 1.25f;
	lostSightMsec		= 1000;

	target				= NULL;
	lostSightTime		= 0;
}

/*
================
idLockOnSystem::Save
================
*/
void idLockOnSystem::Save( idSaveGame *savefile ) const {
	target.Save( savefile );
	savefile->WriteInt( lostSightTime );
}

/*
================
idLockOnSystem::Restore
================
*/
void idLockOnSystem::Restore( idRestoreGame *savefile ) {
	target.Restore( savefile );
	savefile->ReadInt( lostSightTime );
}

/*
================
idLockOnSystem::Configure
================
*/
void idLockOnSystem::Configure( const idDict &args ) {
	maxDistance			= args.GetFloat( "lockon_max_distance", "1024" );
	maxAngle			= args.GetFloat( "lockon_max_angle", "60" );
	angleWeight			= args.GetFloat( "lockon_angle_weight", "1" );
	distanceWeight		= args.GetFloat( "lockon_distance_weight", "0.5" );
	keepDistanceScale	= args.GetFloat( "lockon_keep_distance_scale", "1.25" );
	lostSightMsec		= args.GetInt( "lockon_lost_sight_msec", "1000" );
}

/*
================
idLockOnSystem::GetTargetPoint
================
*/
idVec3 idLockOnSystem::GetTargetPoint( const idActor *actor ) {
	return actor->GetPhysics()->GetAbsBounds().GetCenter();
}

/*
================
idLockOnSystem::IsValidTarget

Distance and line of sight are checked separately.
================
*/
bool idLockOnSystem::IsValidTarget( const idPlayer *owner, const idActor *actor ) const {
	if ( actor == NULL || actor == owner || !actor->IsType( idAI::Type ) ) {
		return false;
	}
	if ( actor->health <= 0 || actor->IsHidden() || actor->fl.isDormant ) {
		return false;
	}
	// friendly characters are idAI too
	return actor->team != owner->team;
}

/*
================
idLockOnSystem::HasLineOfSight

Only world geometry blocks it, other monsters in between don't (MASK_OPAQUE has no CONTENTS_BODY).
================
*/
bool idLockOnSystem::HasLineOfSight( const idPlayer *owner, const idActor *actor ) const {
	trace_t tr;

	gameLocal.clip.TracePoint( tr, owner->GetEyePosition(), GetTargetPoint( actor ), MASK_OPAQUE, owner );
	return tr.fraction >= 1.0f || gameLocal.GetTraceEntity( tr ) == actor;
}

/*
================
idLockOnSystem::Acquire
================
*/
bool idLockOnSystem::Acquire( idPlayer *owner, const idVec3 &viewOrigin, const idMat3 &viewAxis ) {
	const idVec3 eye = owner->GetEyePosition();
	const float cosMaxAngle = idMath::Cos( DEG2RAD( maxAngle ) );
	idActor *best = NULL;
	float bestScore = idMath::INFINITY;

	Release();

	for ( idEntity *ent = gameLocal.activeEntities.Next(); ent != NULL; ent = ent->activeNode.Next() ) {
		if ( !ent->IsType( idAI::Type ) ) {
			continue;
		}
		idActor *actor = static_cast<idActor *>( ent );
		if ( !IsValidTarget( owner, actor ) ) {
			continue;
		}

		const idVec3 point = GetTargetPoint( actor );
		const float dist = ( point - eye ).Length();
		if ( dist > maxDistance ) {
			continue;
		}

		idVec3 dir = point - viewOrigin;
		dir.Normalize();
		const float cosAngle = dir * viewAxis[0];
		if ( cosAngle < cosMaxAngle ) {
			continue;
		}

		const float score = angleWeight * ( 1.0f - cosAngle ) + distanceWeight * ( dist / maxDistance );
		if ( score < bestScore ) {
			bestScore = score;
			best = actor;
		}
	}

	// a single trace, for the winner only
	if ( best == NULL || !HasLineOfSight( owner, best ) ) {
		return false;
	}

	target = best;
	lostSightTime = 0;
	return true;
}

/*
================
idLockOnSystem::Release
================
*/
void idLockOnSystem::Release( void ) {
	target = NULL;
	lostSightTime = 0;
}

/*
================
idLockOnSystem::Update
================
*/
void idLockOnSystem::Update( idPlayer *owner ) {
	idActor *actor = target.GetEntity();
	if ( actor == NULL ) {
		lostSightTime = 0;
		return;
	}

	if ( !IsValidTarget( owner, actor ) || ( GetTargetPoint( actor ) - owner->GetEyePosition() ).LengthSqr() > Square( maxDistance * keepDistanceScale ) ) {
		Release();
		return;
	}

	if ( HasLineOfSight( owner, actor ) ) {
		lostSightTime = 0;
	} else if ( lostSightTime == 0 ) {
		lostSightTime = gameLocal.time;
	} else if ( gameLocal.time - lostSightTime > lostSightMsec ) {
		Release();
	}
}

/*
===============================================================================

	idThirdPersonCamera

===============================================================================
*/

/*
================
idThirdPersonCamera::idThirdPersonCamera
================
*/
idThirdPersonCamera::idThirdPersonCamera( void ) {
	distance		= 110.0f;
	shoulderOffset	= 0.0f;
	pivotUp			= 8.0f;
	pivotSmoothTime	= 0.08f;
	returnSpeed		= 4.0f;
	lockYawSpeed	= 6.0f;
	lockPitchSpeed	= 4.0f;
	lockBlendSpeed	= 8.0f;
	minPitch		= -50.0f;
	maxPitch		= 70.0f;

	initialized		= false;
	pivot.Zero();
	pivotVelocity.Zero();
	curFraction		= 1.0f;
	lockBlend		= 0.0f;
	lockPoint.Zero();
}

/*
================
idThirdPersonCamera::Save

Only the state, the settings come from the player def again on spawn.
================
*/
void idThirdPersonCamera::Save( idSaveGame *savefile ) const {
	savefile->WriteBool( initialized );
	savefile->WriteVec3( pivot );
	savefile->WriteVec3( pivotVelocity );
	savefile->WriteFloat( curFraction );
	savefile->WriteFloat( lockBlend );
	savefile->WriteVec3( lockPoint );
}

/*
================
idThirdPersonCamera::Restore
================
*/
void idThirdPersonCamera::Restore( idRestoreGame *savefile ) {
	savefile->ReadBool( initialized );
	savefile->ReadVec3( pivot );
	savefile->ReadVec3( pivotVelocity );
	savefile->ReadFloat( curFraction );
	savefile->ReadFloat( lockBlend );
	savefile->ReadVec3( lockPoint );
}

/*
================
idThirdPersonCamera::Configure
================
*/
void idThirdPersonCamera::Configure( const idDict &args ) {
	distance		= args.GetFloat( "cam_distance", "110" );
	shoulderOffset	= args.GetFloat( "cam_shoulder_offset", "0" );
	pivotUp			= args.GetFloat( "cam_pivot_up", "8" );
	pivotSmoothTime	= args.GetFloat( "cam_pivot_smooth_time", "0.08" );
	returnSpeed		= args.GetFloat( "cam_return_speed", "4" );
	lockYawSpeed	= args.GetFloat( "cam_lock_yaw_speed", "6" );
	lockPitchSpeed	= args.GetFloat( "cam_lock_pitch_speed", "4" );
	lockBlendSpeed	= args.GetFloat( "cam_lock_blend_speed", "8" );
	minPitch		= args.GetFloat( "cam_min_pitch", "-50" );
	maxPitch		= args.GetFloat( "cam_max_pitch", "70" );
}

/*
================
idThirdPersonCamera::Update
================
*/
void idThirdPersonCamera::Update( idPlayer *owner, renderView_t *rv ) {
	const float dt = MS2SEC( gameLocal.msec );
	const idBounds camBounds( idVec3( -CAM_COLLISION_SIZE, -CAM_COLLISION_SIZE, -CAM_COLLISION_SIZE ), idVec3( CAM_COLLISION_SIZE, CAM_COLLISION_SIZE, CAM_COLLISION_SIZE ) );
	const idMat3 &gravityAxis = owner->GetGravityAxis();
	trace_t tr;
	idVec3 eye;
	idMat3 eyeAxis;

	// pivot: follows the eyes with a spring, so steps, crouching and landings don't jerk the view
	owner->GetViewPos( eye, eyeAxis );
	const idVec3 wantedPivot = eye - owner->GetPhysics()->GetGravityNormal() * pivotUp;

	if ( !initialized || ( wantedPivot - pivot ).LengthSqr() > Square( CAM_SNAP_DISTANCE ) ) {
		pivot = wantedPivot;
		pivotVelocity.Zero();
		curFraction = 1.0f;		// the collision trace below still pulls it in on this frame
		initialized = true;
	} else {
		CamSmoothDamp( pivot, pivotVelocity, wantedPivot, pivotSmoothTime, dt );
	}

	// the lagging pivot or pivotUp could end up in a wall or ceiling: keep it where the eyes can reach
	gameLocal.clip.TraceBounds( tr, eye, pivot, camBounds, MASK_SOLID, owner );
	pivot = tr.endpos;

	// lock-on: turn the camera behind the player, facing the target
	idActor *target = owner->GetLockOn().GetTarget();
	idAngles angles = owner->GetCameraAngles();

	lockBlend += ( ( target != NULL ? 1.0f : 0.0f ) - lockBlend ) * CamExpBlend( lockBlendSpeed, dt );

	if ( target != NULL ) {
		lockPoint = idLockOnSystem::GetTargetPoint( target );

		const idVec3 toTarget = ( lockPoint - pivot ) * gravityAxis.Transpose();
		idAngles lockAngles = toTarget.ToAngles();
		lockAngles.pitch = idMath::ClampFloat( CAM_LOCK_PITCH_MIN, CAM_LOCK_PITCH_MAX, lockAngles.pitch * 0.5f + CAM_LOCK_PITCH_BIAS );

		angles.yaw = idMath::AngleNormalize180( angles.yaw + idMath::AngleDelta( lockAngles.yaw, angles.yaw ) * CamExpBlend( lockYawSpeed, dt ) );
		angles.pitch += ( lockAngles.pitch - angles.pitch ) * CamExpBlend( lockPitchSpeed, dt );
		angles.roll = 0.0f;

		// through the delta angles, so the mouse keeps working from here
		owner->SetCameraAngles( angles );
	}

	// orbit position, pulled in front of anything solid. Pulling in is instant, going back out is eased
	const idMat3 axis = angles.ToMat3() * gravityAxis;
	const idVec3 wanted = pivot - axis[0] * distance - axis[1] * shoulderOffset;

	gameLocal.clip.TraceBounds( tr, pivot, wanted, camBounds, MASK_SOLID, owner );
	if ( tr.fraction < curFraction ) {
		curFraction = tr.fraction;
	} else {
		curFraction += ( tr.fraction - curFraction ) * CamExpBlend( returnSpeed, dt );
	}

	rv->vieworg = pivot + ( wanted - pivot ) * curFraction;
	rv->viewaxis = axis;

	// frame the player and the target together: look at the point between them
	if ( lockBlend > 0.001f ) {
		idVec3 lockDir = ( lockPoint + pivot ) * 0.5f - rv->vieworg;
		if ( lockDir.Normalize() > 1.0f ) {
			idVec3 dir = axis[0] + ( lockDir - axis[0] ) * lockBlend;
			if ( dir.Normalize() > 0.001f ) {
				rv->viewaxis = dir.ToMat3();
			}
		}
	}

	// not the owner's view id: the player body and head are drawn, the first person view weapon is not
	rv->viewID = 0;
}
