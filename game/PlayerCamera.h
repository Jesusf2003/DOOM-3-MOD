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

#ifndef __GAME_PLAYERCAMERA_H__
#define __GAME_PLAYERCAMERA_H__

/*
===============================================================================

	Action (hack & slash) third person camera and target lock-on.

	Both are plain members of idPlayer, not entities: they don't use an entity
	slot, don't think on their own and are saved with the player.

	The camera orbits around a smoothed pivot at the player's eyes using the
	player's cameraAngles (mouse / stick), pulls in when the world is between
	the pivot and the camera, and turns towards the lock-on target when there is one.

===============================================================================
*/

class idPlayer;
class idActor;

/*
===============================================================================

	idLockOnSystem

===============================================================================
*/

class idLockOnSystem {
public:
							idLockOnSystem( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

							// "lockon_*" keys of the player def
	void					Configure( const idDict &args );

							// picks the living enemy closest to the center of the screen, weighted by distance.
							// The angle is measured from the camera, the distance and the line of sight from the eyes
	bool					Acquire( idPlayer *owner, const idVec3 &viewOrigin, const idMat3 &viewAxis );
	void					Release( void );
							// drops the target when it dies, disappears, goes out of range or stays out of sight too long
	void					Update( idPlayer *owner );

	idActor *				GetTarget( void ) const { return target.GetEntity(); }
	bool					IsLocked( void ) const { return target.GetEntity() != NULL; }

							// the point the camera and the aim look at
	static idVec3			GetTargetPoint( const idActor *actor );

private:
	bool					IsValidTarget( const idPlayer *owner, const idActor *actor ) const;
	bool					HasLineOfSight( const idPlayer *owner, const idActor *actor ) const;

	float					maxDistance;		// candidates further than this from the eyes are ignored
	float					maxAngle;			// degrees from the center of the screen
	float					angleWeight;		// score = angleWeight * ( 1 - cos ) + distanceWeight * dist / maxDistance, lowest wins
	float					distanceWeight;
	float					keepDistanceScale;	// a locked target is only dropped past maxDistance * this
	int						lostSightMsec;		// a locked target out of sight for this long is dropped

	idEntityPtr<idActor>	target;
	int						lostSightTime;		// gameLocal.time the target went out of sight, 0 while it is visible
};

/*
===============================================================================

	idThirdPersonCamera

===============================================================================
*/

class idThirdPersonCamera {
public:
							idThirdPersonCamera( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

							// "cam_*" keys of the player def
	void					Configure( const idDict &args );
							// next Update snaps to the player instead of easing in (spawn, teleport, mode switch)
	void					Reset( void ) { initialized = false; }

							// computes the camera of this frame into rv->vieworg / rv->viewaxis. While the owner
							// has a lock-on target, it also steers the owner's camera angles towards it
	void					Update( idPlayer *owner, renderView_t *rv );

	void					SetDistance( float d ) { distance = d; }
	void					SetShoulderOffset( float o ) { shoulderOffset = o; }
	void					SetPivotUp( float u ) { pivotUp = u; }
	void					SetLockYawSpeed( float s ) { lockYawSpeed = s; }
	void					SetLockPitchSpeed( float s ) { lockPitchSpeed = s; }
	float					GetDistance( void ) const { return distance; }
	float					GetShoulderOffset( void ) const { return shoulderOffset; }
	float					GetPivotUp( void ) const { return pivotUp; }
	float					GetLockYawSpeed( void ) const { return lockYawSpeed; }
	float					GetLockPitchSpeed( void ) const { return lockPitchSpeed; }
	float					GetMinPitch( void ) const { return minPitch; }
	float					GetMaxPitch( void ) const { return maxPitch; }

private:
							// settings
	float					distance;			// from the pivot to the camera, before collision
	float					shoulderOffset;		// sideways, positive puts the camera over the right shoulder
	float					pivotUp;			// pivot height above the eyes
	float					pivotSmoothTime;	// seconds for the pivot to catch up with the eyes
	float					returnSpeed;		// 1/s, how fast the camera moves back out once a wall stops blocking it
	float					lockYawSpeed;		// 1/s, how fast the lock-on turns the camera yaw
	float					lockPitchSpeed;		// 1/s, how fast the lock-on turns the camera pitch
	float					lockBlendSpeed;		// 1/s, fade between the free view and the view framing the target
	float					minPitch;			// free camera pitch limits (positive looks down)
	float					maxPitch;

							// state
	bool					initialized;
	idVec3					pivot;
	idVec3					pivotVelocity;
	float					curFraction;		// how far the camera is along the pivot -> wanted position segment
	float					lockBlend;			// 0 free view, 1 framing the target
	idVec3					lockPoint;			// last point of the target, kept to fade out after a release
};

#endif /* !__GAME_PLAYERCAMERA_H__ */
