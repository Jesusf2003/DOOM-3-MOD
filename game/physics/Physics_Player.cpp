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
#include "Entity.h"

#include "physics/Physics_Player.h"

CLASS_DECLARATION( idPhysics_Actor, idPhysics_Player )
END_CLASS

// movement parameters
const float PM_STOPSPEED		= 100.0f;
const float PM_SWIMSCALE		= 0.5f;
const float PM_LADDERSPEED		= 100.0f;
const float PM_STEPSCALE		= 1.0f;

const float PM_ACCELERATE		= 10.0f;
const float PM_AIRACCELERATE	= 1.0f;
const float PM_WATERACCELERATE	= 4.0f;
const float PM_FLYACCELERATE	= 8.0f;

const float PM_FRICTION			= 6.0f;
const float PM_AIRFRICTION		= 0.0f;
const float PM_WATERFRICTION	= 1.0f;
const float PM_FLYFRICTION		= 3.0f;
const float PM_NOCLIPFRICTION	= 12.0f;

const float MIN_WALK_NORMAL		= 0.7f;		// can't walk on very steep slopes
const float OVERCLIP			= 1.001f;

// jump assists
const int PM_COYOTE_MSEC		= 150;		// grace time to jump after walking off a ledge
const int PM_JUMPBUFFER_MSEC	= 150;		// a jump pressed this long before landing still fires
const float PM_FALL_GRAVITY_SCALE	= 1.8f;	// gravity multiplier while falling, removes the floaty jump

// slide
const int PM_SLIDE_DURATION_MSEC	= 600;		// slide length on flat ground
const float PM_SLIDE_SPEED_SCALE	= 1.25f;	// initial slide speed = sprint speed * this
const float PM_SLIDE_FRICTION		= 1.0f;		// fraction of speed lost per second, much lower than PM_FRICTION
const float PM_SLIDE_STEER_SPEED	= 60.0f;	// max sideways speed from strafe keys while sliding
const float PM_SLIDE_TURN_RATE		= 30.0f;	// degrees per second slideDir can follow the view
const float PM_SLIDE_SLOPE_SCALE	= 1.0f;		// how much of the gravity along a ramp accelerates the slide
const float PM_SLIDE_MIN_SLOPE_ACCEL	= 100.0f;	// downhill acceleration (u/s^2, ~5 degrees) that keeps the slide going
const int PM_SLIDE_SLOPE_GRACE_MSEC	= 100;		// slide keeps going this long after leaving a steep enough ramp
const float PM_SLIDE_MAX_SPEED		= 600.0f;	// cap for long ramps
const float PM_SLIDE_MIN_SPEED_FRAC	= 0.85f;	// must already move at this fraction of sprint speed to slide
const int PM_SLIDE_COOLDOWN_MSEC	= 500;		// time after a slide ends before another one can start
const float PM_SLIDE_LAND_SPEED_SCALE	= 1.3f;	// landing with crouch held slides if faster than pm_walkspeed * this

// vault / ledge grab, heights are measured from the feet
const int PM_VAULT_INPUT_BUFFER_MSEC	= 250;		// a jump press can start a vault for this long; holding jump in the air renews it
const float PM_VAULT_HOLD_MIN_UPSPEED	= -60.0f;	// holding jump keeps searching while rising and around the apex (falling slower than this)
const float PM_VAULT_WAIST_HEIGHT	= 22.0f;	// lower probe
const float PM_VAULT_HEAD_HEIGHT	= 68.0f;	// upper probe
const float PM_VAULT_TOP_HEIGHT		= 92.0f;	// above the head, must be free to grab a high ledge
const float PM_VAULT_MIN_DIST		= 16.0f;	// ledge grab: the face must be at least this far from the player origin...
const float PM_VAULT_MAX_DIST		= 40.0f;	// ...and at most this far (0..24u in front of the player box)
const float PM_VAULT_LOW_MIN_DIST	= 0.0f;		// low vault: gap between the FRONT of the player box and the face, at least (0 = touching)...
const float PM_VAULT_LOW_MAX_DIST	= 16.0f;	// ...and at most (measured from the box because the origin is always 16u behind it)
const float PM_VAULT_MAX_ANGLE_COS	= 0.7071f;	// forward . -wallNormal, cos( 45 degrees ): wider approach angles are rejected
const float PM_VAULT_MAX_WALL_SLOPE	= 0.3f;		// the obstacle face must be close to vertical (|normal . up| below this)
const float PM_VAULT_LEDGE_INSET	= 6.0f;		// the top is probed this far behind the face
const float PM_VAULT_MIN_LEDGE_NORMAL	= 0.7f;	// the top must be walkable
const float PM_VAULT_MIN_HEIGHT		= 24.0f;	// lower obstacles are left to step up / a regular jump
const float PM_VAULT_LOW_MAX_HEIGHT	= 56.0f;	// up to this height: low vault (continuous pass), above: ledge grab
const float PM_VAULT_LAND_INSET		= 2.0f;		// the back of the box ends this far past the edge
const float PM_VAULT_TARGET_CLEARANCE	= 2.0f;		// kinematic moves end this far above the top, gravity settles the rest
const int PM_VAULT_COOLDOWN_MSEC	= 300;		// no new vault right after one ends
const float PM_VAULT_EXIT_BOOST	= 1.15f;	// low vault: run-up speed handed back times this (never above pm_runspeed times this)
const float PM_VAULT_LAND_EXTEND_MAX	= 48.0f;	// low vault: the landing may be pushed this far past the edge...
const float PM_VAULT_LAND_EXTEND_STEP	= 8.0f;		// ...backing off in steps of this when there's no room
const int PM_VAULT_GRACE_FRAMES	= 3;		// low vault: grounded frames without ground friction after the landing
const int PM_VAULT_GRACE_MAX_MSEC	= 250;		// that grace is dropped if the ground isn't reached within this
const int PM_VAULT_OVERTIME_MSEC	= 150;		// extra time for a low vault to reach its landing spot if the arc was blocked
const float PM_VAULT_SWEEP_STEP		= 4.0f;		// the kinematic move of a frame is swept in substeps of at most this length
const float PM_VAULT_CEILING_GAP	= 1.0f;		// crouched under a low ceiling, the probes run this far below it
const float PM_VAULT_START_HEADROOM	= 1.0f;		// a vault never starts with the box closer than this to a ceiling
const float PM_VAULT_FIT_MARGIN		= 1.0f;		// the box must fit at the landing spot with this much room on every side
const float PM_VAULT_OPENING_STEP	= 6.0f;		// height step of the probes that look for an opening in a wall (vent, duct)
const float PM_VAULT_NUDGE_MAX		= 2.0f;		// a box caught in overlapping brushes may be pushed this far back out of the face

// low vault (VAULT_LOW). Duration follows horizontal speed at takeoff; low speed and crouching
// take longer, while sprinting carries the player across without losing momentum.
const float PM_VAULT_LOW_SLOW_SPEED		= 90.0f;
const float PM_VAULT_LOW_WALK_SPEED		= 140.0f;
const float PM_VAULT_LOW_SPRINT_SPEED	= 200.0f;
const int PM_VAULT_LOW_CROUCH_MSEC		= 490;
const int PM_VAULT_LOW_WALK_MSEC			= 405;
const int PM_VAULT_LOW_SPRINT_MSEC		= 200;
const float PM_VAULT_LOW_ARC		= 5.0f;		// H_clearance: the arc peaks this far above the edge (kept flat: a glide, not a hop)
const float PM_VAULT_BEZIER_MAX_DIP	= 5.0f;		// low vault: the pass may pull back this far while rising, to clear the edge before the face

// ledge grab (VAULT_HIGH_GRAB -> VAULT_CLIMBING -> VAULT_MANTLE). The total of the three phases only
// depends on the ledge height and is split between them with the fractions below
const int PM_VAULT_HIGH_DURATION_MSEC		= 650;	// total at the lowest ledge (just above PM_VAULT_LOW_MAX_HEIGHT)...
const int PM_VAULT_HIGH_DURATION_MAX_MSEC	= 850;	// ...growing linearly up to this at PM_VAULT_TOP_HEIGHT
const float PM_VAULT_ABSORB_FRAC	= 0.15f;	// phase 1 share: the hands hit the ledge
const float PM_VAULT_MANTLE_FRAC	= 0.25f;	// phase 3 share: EaseInQuad over the edge. Phase 2 (EaseOutCubic pull up) gets the rest
const float PM_VAULT_ABSORB_TAU		= 0.035f;	// phase 1: time constant (seconds) of the exponential braking
const float PM_VAULT_MANTLE_OVERLAP	= 8.0f;		// phase 3 ends with the front of the box this far past the edge
const float PM_LEDGE_JUMP_SPEED		= 320.0f;	// upward speed of a ledge jump (fresh jump press during phase 1 or 2)
const float PM_LEDGE_JUMP_FORWARD	= 80.0f;	// forward speed of a ledge jump
const int PM_LEDGE_JUMP_MIN_MSEC	= 100;		// presses this soon after the grab are ignored (double taps are not ledge jumps)

idCVar pm_vaultDebug( "pm_vaultDebug", "0", CVAR_GAME | CVAR_BOOL, "print what the vault detection finds when a vault starts" );

// movementFlags
const int PMF_DUCKED			= 1;		// set when ducking
const int PMF_JUMPED			= 2;		// set when the player jumped this frame
const int PMF_STEPPED_UP		= 4;		// set when the player stepped up this frame
const int PMF_STEPPED_DOWN		= 8;		// set when the player stepped down this frame
const int PMF_JUMP_HELD			= 16;		// set when jump button is held down
const int PMF_TIME_LAND			= 32;		// movementTime is time before rejump
const int PMF_TIME_KNOCKBACK	= 64;		// movementTime is an air-accelerate only time
const int PMF_TIME_WATERJUMP	= 128;		// movementTime is waterjump
const int PMF_ALL_TIMES			= (PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_KNOCKBACK);

int c_pmove = 0;

/*
============
idPhysics_Player::CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
float idPhysics_Player::CmdScale( const usercmd_t &cmd ) const {
	int		max;
	float	total;
	float	scale;
	int		forwardmove;
	int		rightmove;
	int		upmove;

	forwardmove = cmd.forwardmove;
	rightmove = cmd.rightmove;

	// since the crouch key doubles as downward movement, ignore downward movement when we're on the ground
	// otherwise crouch speed will be lower than specified
	if ( walking ) {
		upmove = 0;
	} else {
		upmove = cmd.upmove;
	}

	max = abs( forwardmove );
	if ( abs( rightmove ) > max ) {
		max = abs( rightmove );
	}
	if ( abs( upmove ) > max ) {
		max = abs( upmove );
	}

	if ( !max ) {
		return 0.0f;
	}

	total = idMath::Sqrt( (float) forwardmove * forwardmove + rightmove * rightmove + upmove * upmove );
	scale = (float) playerSpeed * max / ( 127.0f * total );

	return scale;
}

/*
==============
idPhysics_Player::Accelerate

Handles user intended acceleration
==============
*/
void idPhysics_Player::Accelerate( const idVec3 &wishdir, const float wishspeed, const float accel ) {
#if 1
	// q2 style
	float addspeed, accelspeed, currentspeed;

	currentspeed = current.velocity * wishdir;
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0) {
		return;
	}
	accelspeed = accel * frametime * wishspeed;
	if (accelspeed > addspeed) {
		accelspeed = addspeed;
	}

	current.velocity += accelspeed * wishdir;
#else
	// proper way (avoids strafe jump maxspeed bug), but feels bad
	idVec3		wishVelocity;
	idVec3		pushDir;
	float		pushLen;
	float		canPush;

	wishVelocity = wishdir * wishspeed;
	pushDir = wishVelocity - current.velocity;
	pushLen = pushDir.Normalize();

	canPush = accel * frametime * wishspeed;
	if (canPush > pushLen) {
		canPush = pushLen;
	}

	current.velocity += canPush * pushDir;
#endif
}

/*
==================
idPhysics_Player::SlideMove

Returns true if the velocity was clipped in some way
==================
*/
#define	MAX_CLIP_PLANES	5

bool idPhysics_Player::SlideMove( bool gravity, bool stepUp, bool stepDown, bool push ) {
	int			i, j, k, pushFlags;
	int			bumpcount, numbumps, numplanes;
	float		d, time_left, into, totalMass;
	idVec3		dir, planes[MAX_CLIP_PLANES];
	idVec3		end, stepEnd, primal_velocity, endVelocity, endClipVelocity, clipVelocity;
	trace_t		trace, stepTrace, downTrace;
	bool		nearGround, stepped, pushed;

	numbumps = 4;

	primal_velocity = current.velocity;

	if ( gravity ) {
		endVelocity = current.velocity + gravityVector * frametime;
		current.velocity = ( current.velocity + endVelocity ) * 0.5f;
		primal_velocity = endVelocity;
		if ( groundPlane ) {
			// slide along the ground plane
			current.velocity.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );
		}
	}
	else {
		endVelocity = current.velocity;
	}

	time_left = frametime;

	// never turn against the ground plane
	if ( groundPlane ) {
		numplanes = 1;
		planes[0] = groundTrace.c.normal;
	} else {
		numplanes = 0;
	}

	// never turn against original velocity
	planes[numplanes] = current.velocity;
	planes[numplanes].Normalize();
	numplanes++;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {

		// calculate position we are trying to move to
		end = current.origin + time_left * current.velocity;

		// see if we can make it there
		gameLocal.clip.Translation( trace, current.origin, end, clipModel, clipModel->GetAxis(), clipMask, self );

		time_left -= time_left * trace.fraction;
		current.origin = trace.endpos;

		// if moved the entire distance
		if ( trace.fraction >= 1.0f ) {
			break;
		}

		stepped = pushed = false;

		// if we are allowed to step up
		if ( stepUp ) {

			nearGround = groundPlane | ladder;

			if ( !nearGround ) {
				// trace down to see if the player is near the ground
				// step checking when near the ground allows the player to move up stairs smoothly while jumping
				stepEnd = current.origin + maxStepHeight * gravityNormal;
				gameLocal.clip.Translation( downTrace, current.origin, stepEnd, clipModel, clipModel->GetAxis(), clipMask, self );
				nearGround = ( downTrace.fraction < 1.0f && (downTrace.c.normal * -gravityNormal) > MIN_WALK_NORMAL );
			}

			// may only step up if near the ground or on a ladder
			if ( nearGround ) {

				// step up
				stepEnd = current.origin - maxStepHeight * gravityNormal;
				gameLocal.clip.Translation( downTrace, current.origin, stepEnd, clipModel, clipModel->GetAxis(), clipMask, self );

				// trace along velocity
				stepEnd = downTrace.endpos + time_left * current.velocity;
				gameLocal.clip.Translation( stepTrace, downTrace.endpos, stepEnd, clipModel, clipModel->GetAxis(), clipMask, self );

				// step down
				stepEnd = stepTrace.endpos + maxStepHeight * gravityNormal;
				gameLocal.clip.Translation( downTrace, stepTrace.endpos, stepEnd, clipModel, clipModel->GetAxis(), clipMask, self );

				if ( downTrace.fraction >= 1.0f || (downTrace.c.normal * -gravityNormal) > MIN_WALK_NORMAL ) {

					// if moved the entire distance
					if ( stepTrace.fraction >= 1.0f ) {
						time_left = 0;
						current.stepUp -= ( downTrace.endpos - current.origin ) * gravityNormal;
						current.origin = downTrace.endpos;
						current.movementFlags |= PMF_STEPPED_UP;
						current.velocity *= PM_STEPSCALE;
						break;
					}

					// if the move is further when stepping up
					if ( stepTrace.fraction > trace.fraction ) {
						time_left -= time_left * stepTrace.fraction;
						current.stepUp -= ( downTrace.endpos - current.origin ) * gravityNormal;
						current.origin = downTrace.endpos;
						current.movementFlags |= PMF_STEPPED_UP;
						current.velocity *= PM_STEPSCALE;
						trace = stepTrace;
						stepped = true;
					}
				}
			}
		}

		// if we can push other entities and not blocked by the world
		if ( push && trace.c.entityNum != ENTITYNUM_WORLD ) {

			clipModel->SetPosition( current.origin, clipModel->GetAxis() );

			// clip movement, only push idMoveables, don't push entities the player is standing on
			// apply impact to pushed objects
			pushFlags = PUSHFL_CLIP|PUSHFL_ONLYMOVEABLE|PUSHFL_NOGROUNDENTITIES|PUSHFL_APPLYIMPULSE;

			// clip & push
			totalMass = gameLocal.push.ClipTranslationalPush( trace, self, pushFlags, end, end - current.origin );

			if ( totalMass > 0.0f ) {
				// decrease velocity based on the total mass of the objects being pushed ?
				current.velocity *= 1.0f - idMath::ClampFloat( 0.0f, 1000.0f, totalMass - 20.0f ) * ( 1.0f / 950.0f );
				pushed = true;
			}

			current.origin = trace.endpos;
			time_left -= time_left * trace.fraction;

			// if moved the entire distance
			if ( trace.fraction >= 1.0f ) {
				break;
			}
		}

		if ( !stepped ) {
			// let the entity know about the collision
			self->Collide( trace, current.velocity );
		}

		if ( numplanes >= MAX_CLIP_PLANES ) {
			// MrElusive: I think we have some relatively high poly LWO models with a lot of slanted tris
			// where it may hit the max clip planes
			current.velocity = vec3_origin;
			return true;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for ( i = 0; i < numplanes; i++ ) {
			if ( ( trace.c.normal * planes[i] ) > 0.999f ) {
				current.velocity += trace.c.normal;
				break;
			}
		}
		if ( i < numplanes ) {
			continue;
		}
		planes[numplanes] = trace.c.normal;
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for ( i = 0; i < numplanes; i++ ) {
			into = current.velocity * planes[i];
			if ( into >= 0.1f ) {
				continue;		// move doesn't interact with the plane
			}

			// slide along the plane
			clipVelocity = current.velocity;
			clipVelocity.ProjectOntoPlane( planes[i], OVERCLIP );

			// slide along the plane
			endClipVelocity = endVelocity;
			endClipVelocity.ProjectOntoPlane( planes[i], OVERCLIP );

			// see if there is a second plane that the new move enters
			for ( j = 0; j < numplanes; j++ ) {
				if ( j == i ) {
					continue;
				}
				if ( ( clipVelocity * planes[j] ) >= 0.1f ) {
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				clipVelocity.ProjectOntoPlane( planes[j], OVERCLIP );
				endClipVelocity.ProjectOntoPlane( planes[j], OVERCLIP );

				// see if it goes back into the first clip plane
				if ( ( clipVelocity * planes[i] ) >= 0 ) {
					continue;
				}

				// slide the original velocity along the crease
				dir = planes[i].Cross( planes[j] );
				dir.Normalize();
				d = dir * current.velocity;
				clipVelocity = d * dir;

				dir = planes[i].Cross( planes[j] );
				dir.Normalize();
				d = dir * endVelocity;
				endClipVelocity = d * dir;

				// see if there is a third plane the the new move enters
				for ( k = 0; k < numplanes; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}
					if ( ( clipVelocity * planes[k] ) >= 0.1f ) {
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a tripple plane interaction
					current.velocity = vec3_origin;
					return true;
				}
			}

			// if we have fixed all interactions, try another move
			current.velocity = clipVelocity;
			endVelocity = endClipVelocity;
			break;
		}
	}

	// step down
	if ( stepDown && groundPlane ) {
		stepEnd = current.origin + gravityNormal * maxStepHeight;
		gameLocal.clip.Translation( downTrace, current.origin, stepEnd, clipModel, clipModel->GetAxis(), clipMask, self );
		if ( downTrace.fraction > 1e-4f && downTrace.fraction < 1.0f ) {
			current.stepUp -= ( downTrace.endpos - current.origin ) * gravityNormal;
			current.origin = downTrace.endpos;
			current.movementFlags |= PMF_STEPPED_DOWN;
			current.velocity *= PM_STEPSCALE;
		}
	}

	if ( gravity ) {
		current.velocity = endVelocity;
	}

	// come to a dead stop when the velocity orthogonal to the gravity flipped
	clipVelocity = current.velocity - gravityNormal * current.velocity * gravityNormal;
	endClipVelocity = endVelocity - gravityNormal * endVelocity * gravityNormal;
	if ( clipVelocity * endClipVelocity < 0.0f ) {
		current.velocity = gravityNormal * current.velocity * gravityNormal;
	}

	return (bool)( bumpcount == 0 );
}

/*
==================
idPhysics_Player::Friction

Handles both ground friction and water friction
==================
*/
void idPhysics_Player::Friction( void ) {
	idVec3	vel;
	float	speed, newspeed, control;
	float	drop;

	vel = current.velocity;
	if ( walking ) {
		// ignore slope movement, remove all velocity in gravity direction
		vel += (vel * gravityNormal) * gravityNormal;
	}

	speed = vel.Length();
	if ( speed < 1.0f ) {
		// remove all movement orthogonal to gravity, allows for sinking underwater
		if ( fabs( current.velocity * gravityNormal ) < 1e-5f ) {
			current.velocity.Zero();
		} else {
			current.velocity = (current.velocity * gravityNormal) * gravityNormal;
		}
		// FIXME: still have z friction underwater?
		return;
	}

	drop = 0;

	// spectator friction
	if ( current.movementType == PM_SPECTATOR ) {
		drop += speed * PM_FLYFRICTION * frametime;
	}
	// apply ground friction
	else if ( walking && waterLevel <= WATERLEVEL_FEET ) {
		// right after a low vault the landing doesn't brake: the exit speed carries into the run
		if ( vaultGraceFrames > 0 ) {
			if ( gameLocal.time < vaultGraceExpire ) {
				vaultGraceFrames--;
				return;
			}
			vaultGraceFrames = 0;
		}
		// no friction on slick surfaces
		if ( !(groundMaterial && groundMaterial->GetSurfaceFlags() & SURF_SLICK) ) {
			// if getting knocked back, no friction
			if ( !(current.movementFlags & PMF_TIME_KNOCKBACK) ) {
				control = speed < PM_STOPSPEED ? PM_STOPSPEED : speed;
				drop += control * PM_FRICTION * frametime;
			}
		}
	}
	// apply water friction even if just wading
	else if ( waterLevel ) {
		drop += speed * PM_WATERFRICTION * waterLevel * frametime;
	}
	// apply air friction
	else {
		drop += speed * PM_AIRFRICTION * frametime;
	}

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0) {
		newspeed = 0;
	}
	current.velocity *= ( newspeed / speed );
}

/*
===================
idPhysics_Player::WaterJumpMove

Flying out of the water
===================
*/
void idPhysics_Player::WaterJumpMove( void ) {

	// waterjump has no control, but falls
	idPhysics_Player::SlideMove( true, true, false, false );

	// add gravity
	current.velocity += gravityNormal * frametime;
	// if falling down
	if ( current.velocity * gravityNormal > 0.0f ) {
		// cancel as soon as we are falling down again
		current.movementFlags &= ~PMF_ALL_TIMES;
		current.movementTime = 0;
	}
}

/*
===================
idPhysics_Player::WaterMove
===================
*/
void idPhysics_Player::WaterMove( void ) {
	idVec3	wishvel;
	float	wishspeed;
	idVec3	wishdir;
	float	scale;
	float	vel;

	if ( idPhysics_Player::CheckWaterJump() ) {
		idPhysics_Player::WaterJumpMove();
		return;
	}

	idPhysics_Player::Friction();

	scale = idPhysics_Player::CmdScale( command );

	// user intentions
	if ( !scale ) {
		wishvel = gravityNormal * 60; // sink towards bottom
	} else {
		wishvel = scale * (viewForward * command.forwardmove + viewRight * command.rightmove);
		wishvel -= scale * gravityNormal * command.upmove;
	}

	wishdir = wishvel;
	wishspeed = wishdir.Normalize();

	if ( wishspeed > playerSpeed * PM_SWIMSCALE ) {
		wishspeed = playerSpeed * PM_SWIMSCALE;
	}

	idPhysics_Player::Accelerate( wishdir, wishspeed, PM_WATERACCELERATE );

	// make sure we can go up slopes easily under water
	if ( groundPlane && ( current.velocity * groundTrace.c.normal ) < 0.0f ) {
		vel = current.velocity.Length();
		// slide along the ground plane
		current.velocity.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );

		current.velocity.Normalize();
		current.velocity *= vel;
	}

	idPhysics_Player::SlideMove( false, true, false, false );
}

/*
===================
idPhysics_Player::FlyMove
===================
*/
void idPhysics_Player::FlyMove( void ) {
	idVec3	wishvel;
	float	wishspeed;
	idVec3	wishdir;
	float	scale;

	// normal slowdown
	idPhysics_Player::Friction();

	scale = idPhysics_Player::CmdScale( command );

	if ( !scale ) {
		wishvel = vec3_origin;
	} else {
		wishvel = scale * (viewForward * command.forwardmove + viewRight * command.rightmove);
		wishvel -= scale * gravityNormal * command.upmove;
	}

	wishdir = wishvel;
	wishspeed = wishdir.Normalize();

	idPhysics_Player::Accelerate( wishdir, wishspeed, PM_FLYACCELERATE );

	idPhysics_Player::SlideMove( false, false, false, false );
}

/*
===================
idPhysics_Player::AirMove
===================
*/
void idPhysics_Player::AirMove( void ) {
	idVec3		wishvel;
	idVec3		wishdir;
	float		wishspeed;
	float		scale;

	idPhysics_Player::Friction();

	scale = idPhysics_Player::CmdScale( command );

	// project moves down to flat plane
	viewForward -= (viewForward * gravityNormal) * gravityNormal;
	viewRight -= (viewRight * gravityNormal) * gravityNormal;
	viewForward.Normalize();
	viewRight.Normalize();

	wishvel = viewForward * command.forwardmove + viewRight * command.rightmove;
	wishvel -= (wishvel * gravityNormal) * gravityNormal;
	wishdir = wishvel;
	wishspeed = wishdir.Normalize();
	wishspeed *= scale;

	// not on ground, so little effect on velocity
	idPhysics_Player::Accelerate( wishdir, wishspeed, PM_AIRACCELERATE );

	// asymmetric gravity: fall faster than we rise. SlideMove adds 1g, add the rest here
	if ( current.velocity * gravityNormal > 0.0f ) {
		current.velocity += gravityVector * ( ( PM_FALL_GRAVITY_SCALE - 1.0f ) * frametime );
	}

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if ( groundPlane ) {
		current.velocity.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );
	}

	idPhysics_Player::SlideMove( true, false, false, false );
}

/*
===================
idPhysics_Player::WalkMove
===================
*/
void idPhysics_Player::WalkMove( void ) {
	idVec3		wishvel;
	idVec3		wishdir;
	float		wishspeed;
	float		scale;
	float		accelerate;
	idVec3		oldVelocity, vel;
	float		oldVel, newVel;

	if ( waterLevel > WATERLEVEL_WAIST && ( viewForward * groundTrace.c.normal ) > 0.0f ) {
		// begin swimming
		idPhysics_Player::WaterMove();
		return;
	}

	if ( idPhysics_Player::CheckJump() ) {
		// jumped away
		if ( waterLevel > WATERLEVEL_FEET ) {
			idPhysics_Player::WaterMove();
		}
		else {
			idPhysics_Player::AirMove();
		}
		return;
	}

	idPhysics_Player::Friction();

	scale = idPhysics_Player::CmdScale( command );

	// project moves down to flat plane
	viewForward -= (viewForward * gravityNormal) * gravityNormal;
	viewRight -= (viewRight * gravityNormal) * gravityNormal;

	// project the forward and right directions onto the ground plane
	viewForward.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );
	viewRight.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );
	//
	viewForward.Normalize();
	viewRight.Normalize();

	wishvel = viewForward * command.forwardmove + viewRight * command.rightmove;
	wishdir = wishvel;
	wishspeed = wishdir.Normalize();
	wishspeed *= scale;

	// clamp the speed lower if wading or walking on the bottom
	if ( waterLevel ) {
		float	waterScale;

		waterScale = waterLevel / 3.0f;
		waterScale = 1.0f - ( 1.0f - PM_SWIMSCALE ) * waterScale;
		if ( wishspeed > playerSpeed * waterScale ) {
			wishspeed = playerSpeed * waterScale;
		}
	}

	// when a player gets hit, they temporarily lose full control, which allows them to be moved a bit
	if ( ( groundMaterial && groundMaterial->GetSurfaceFlags() & SURF_SLICK ) || current.movementFlags & PMF_TIME_KNOCKBACK ) {
		accelerate = PM_AIRACCELERATE;
	}
	else {
		accelerate = PM_ACCELERATE;
	}

	idPhysics_Player::Accelerate( wishdir, wishspeed, accelerate );

	if ( ( groundMaterial && groundMaterial->GetSurfaceFlags() & SURF_SLICK ) || current.movementFlags & PMF_TIME_KNOCKBACK ) {
		current.velocity += gravityVector * frametime;
	}

	oldVelocity = current.velocity;

	// slide along the ground plane
	current.velocity.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );

	// if not clipped into the opposite direction
	if ( oldVelocity * current.velocity > 0.0f ) {
		newVel = current.velocity.LengthSqr();
		if ( newVel > 1.0f ) {
			oldVel = oldVelocity.LengthSqr();
			if ( oldVel > 1.0f ) {
				// don't decrease velocity when going up or down a slope
				current.velocity *= idMath::Sqrt( oldVel / newVel );
			}
		}
	}

	// don't do anything if standing still
	vel = current.velocity - (current.velocity * gravityNormal) * gravityNormal;
	if ( !vel.LengthSqr() ) {
		return;
	}

	gameLocal.push.InitSavingPushedEntityPositions();

	idPhysics_Player::SlideMove( false, true, true, true );
}

/*
==============
idPhysics_Player::DeadMove
==============
*/
void idPhysics_Player::DeadMove( void ) {
	float	forward;

	if ( !walking ) {
		return;
	}

	// extra friction
	forward = current.velocity.Length();
	forward -= 20;
	if ( forward <= 0 ) {
		current.velocity = vec3_origin;
	}
	else {
		current.velocity.Normalize();
		current.velocity *= forward;
	}
}

/*
===============
idPhysics_Player::NoclipMove
===============
*/
void idPhysics_Player::NoclipMove( void ) {
	float		speed, drop, friction, newspeed, stopspeed;
	float		scale, wishspeed;
	idVec3		wishdir;

	// friction
	speed = current.velocity.Length();
	if ( speed < 20.0f ) {
		current.velocity = vec3_origin;
	}
	else {
		stopspeed = playerSpeed * 0.3f;
		if ( speed < stopspeed ) {
			speed = stopspeed;
		}
		friction = PM_NOCLIPFRICTION;
		drop = speed * friction * frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0) {
			newspeed = 0;
		}

		current.velocity *= newspeed / speed;
	}

	// accelerate
	scale = idPhysics_Player::CmdScale( command );

	wishdir = scale * (viewForward * command.forwardmove + viewRight * command.rightmove);
	wishdir -= scale * gravityNormal * command.upmove;
	wishspeed = wishdir.Normalize();
	wishspeed *= scale;

	idPhysics_Player::Accelerate( wishdir, wishspeed, PM_ACCELERATE );

	// move
	current.origin += frametime * current.velocity;
}

/*
===============
idPhysics_Player::SpectatorMove
===============
*/
void idPhysics_Player::SpectatorMove( void ) {
	idVec3	wishvel;
	float	wishspeed;
	idVec3	wishdir;
	float	scale;

	idVec3	end;

	// fly movement

	idPhysics_Player::Friction();

	scale = idPhysics_Player::CmdScale( command );

	if ( !scale ) {
		wishvel = vec3_origin;
	} else {
		wishvel = scale * (viewForward * command.forwardmove + viewRight * command.rightmove);
	}

	wishdir = wishvel;
	wishspeed = wishdir.Normalize();

	idPhysics_Player::Accelerate( wishdir, wishspeed, PM_FLYACCELERATE );

	idPhysics_Player::SlideMove( false, false, false, false );
}

/*
============
idPhysics_Player::LadderMove
============
*/
void idPhysics_Player::LadderMove( void ) {
	idVec3	wishdir, wishvel, right;
	float	wishspeed, scale;
	float	upscale;

	// stick to the ladder
	wishvel = -100.0f * ladderNormal;
	current.velocity = (gravityNormal * current.velocity) * gravityNormal + wishvel;

	upscale = (-gravityNormal * viewForward + 0.5f) * 2.5f;
	if ( upscale > 1.0f ) {
		upscale = 1.0f;
	}
	else if ( upscale < -1.0f ) {
		upscale = -1.0f;
	}

	scale = idPhysics_Player::CmdScale( command );
	wishvel = -0.9f * gravityNormal * upscale * scale * (float)command.forwardmove;

	// strafe
	if ( command.rightmove ) {
		// right vector orthogonal to gravity
		right = viewRight - (gravityNormal * viewRight) * gravityNormal;
		// project right vector into ladder plane
		right = right - (ladderNormal * right) * ladderNormal;
		right.Normalize();

		// if we are looking away from the ladder, reverse the right vector
		if ( ladderNormal * viewForward > 0.0f ) {
			right = -right;
		}
		wishvel += 2.0f * right * scale * (float) command.rightmove;
	}

	// up down movement
	if ( command.upmove ) {
		wishvel += -0.5f * gravityNormal * scale * (float) command.upmove;
	}

	// do strafe friction
	idPhysics_Player::Friction();

	// accelerate
	wishspeed = wishvel.Normalize();
	idPhysics_Player::Accelerate( wishvel, wishspeed, PM_ACCELERATE );

	// cap the vertical velocity
	upscale = current.velocity * -gravityNormal;
	if ( upscale < -PM_LADDERSPEED ) {
		current.velocity += gravityNormal * (upscale + PM_LADDERSPEED);
	}
	else if ( upscale > PM_LADDERSPEED ) {
		current.velocity += gravityNormal * (upscale - PM_LADDERSPEED);
	}

	if ( (wishvel * gravityNormal) == 0.0f ) {
		if ( current.velocity * gravityNormal < 0.0f ) {
			current.velocity += gravityVector * frametime;
			if ( current.velocity * gravityNormal > 0.0f ) {
				current.velocity -= (gravityNormal * current.velocity) * gravityNormal;
			}
		}
		else {
			current.velocity -= gravityVector * frametime;
			if ( current.velocity * gravityNormal < 0.0f ) {
				current.velocity -= (gravityNormal * current.velocity) * gravityNormal;
			}
		}
	}

	idPhysics_Player::SlideMove( false, ( command.forwardmove > 0 ), false, false );
}

/*
=============
idPhysics_Player::CorrectAllSolid
=============
*/
void idPhysics_Player::CorrectAllSolid( trace_t &trace, int contents ) {
	if ( debugLevel ) {
		gameLocal.Printf( "%i:allsolid\n", c_pmove );
	}

	// FIXME: jitter around to find a free spot ?

	if ( trace.fraction >= 1.0f ) {
		memset( &trace, 0, sizeof( trace ) );
		trace.endpos = current.origin;
		trace.endAxis = clipModelAxis;
		trace.fraction = 0.0f;
		trace.c.dist = current.origin.z;
		trace.c.normal.Set( 0, 0, 1 );
		trace.c.point = current.origin;
		trace.c.entityNum = ENTITYNUM_WORLD;
		trace.c.id = 0;
		trace.c.type = CONTACT_TRMVERTEX;
		trace.c.material = NULL;
		trace.c.contents = contents;
	}
}

/*
=============
idPhysics_Player::CheckGround
=============
*/
void idPhysics_Player::CheckGround( void ) {
	int i, contents;
	idVec3 point;
	bool hadGroundContacts;

	hadGroundContacts = HasGroundContacts();

	// set the clip model origin before getting the contacts
	clipModel->SetPosition( current.origin, clipModel->GetAxis() );

	EvaluateContacts();

	// setup a ground trace from the contacts
	groundTrace.endpos = current.origin;
	groundTrace.endAxis = clipModel->GetAxis();
	if ( contacts.Num() ) {
		groundTrace.fraction = 0.0f;
		groundTrace.c = contacts[0];
		for ( i = 1; i < contacts.Num(); i++ ) {
			groundTrace.c.normal += contacts[i].normal;
		}
		groundTrace.c.normal.Normalize();
	} else {
		groundTrace.fraction = 1.0f;
	}

	contents = gameLocal.clip.Contents( current.origin, clipModel, clipModel->GetAxis(), -1, self );
	if ( contents & MASK_SOLID ) {
		// do something corrective if stuck in solid
		idPhysics_Player::CorrectAllSolid( groundTrace, contents );
	}

	// if the trace didn't hit anything, we are in free fall
	if ( groundTrace.fraction == 1.0f ) {
		groundPlane = false;
		walking = false;
		groundEntityPtr = NULL;
		return;
	}

	groundMaterial = groundTrace.c.material;
	groundEntityPtr = gameLocal.entities[ groundTrace.c.entityNum ];

	// check if getting thrown off the ground
	if ( (current.velocity * -gravityNormal) > 0.0f && ( current.velocity * groundTrace.c.normal ) > 10.0f ) {
		if ( debugLevel ) {
			gameLocal.Printf( "%i:kickoff\n", c_pmove );
		}

		groundPlane = false;
		walking = false;
		return;
	}

	// slopes that are too steep will not be considered onground
	if ( ( groundTrace.c.normal * -gravityNormal ) < MIN_WALK_NORMAL ) {
		if ( debugLevel ) {
			gameLocal.Printf( "%i:steep\n", c_pmove );
		}

		// FIXME: if they can't slide down the slope, let them walk (sharp crevices)

		// make sure we don't die from sliding down a steep slope
		if ( current.velocity * gravityNormal > 150.0f ) {
			current.velocity -= ( current.velocity * gravityNormal - 150.0f ) * gravityNormal;
		}

		groundPlane = true;
		walking = false;
		return;
	}

	groundPlane = true;
	walking = true;

	// hitting solid ground will end a waterjump
	if ( current.movementFlags & PMF_TIME_WATERJUMP ) {
		current.movementFlags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND );
		current.movementTime = 0;
	}

	// if the player didn't have ground contacts the previous frame
	if ( !hadGroundContacts ) {

		// don't do landing time if we were just going down a slope
		if ( (current.velocity * -gravityNormal) < -200.0f ) {
			// don't allow another jump for a little while
			current.movementFlags |= PMF_TIME_LAND;
			current.movementTime = 250;
		}
	}

	// let the entity know about the collision
	self->Collide( groundTrace, current.velocity );

	if ( groundEntityPtr.GetEntity() ) {
		impactInfo_t info;
		groundEntityPtr.GetEntity()->GetImpactInfo( self, groundTrace.c.id, groundTrace.c.point, &info );
		if ( info.invMass != 0.0f ) {
			groundEntityPtr.GetEntity()->ApplyImpulse( self, groundTrace.c.id, groundTrace.c.point, current.velocity / ( info.invMass * 10.0f ) );
		}
	}
}

/*
==============
idPhysics_Player::CheckDuck

Sets clip model size
==============
*/
void idPhysics_Player::CheckDuck( void ) {
	float maxZ;

	if ( current.movementType == PM_DEAD ) {
		maxZ = pm_deadheight.GetFloat();
	} else {
		// stand up when up against a ladder. A slide keeps us down even if crouch is released, and so
		// does a latched crouch (pm_toggleCrouch) while jump is pressed: that jump is a crouch jump
		if ( ( command.upmove < 0 || isSliding || crouchLatched ) && !ladder ) {
			if ( !( current.movementFlags & PMF_DUCKED ) ) {
				current.movementFlags |= PMF_DUCKED;
				if ( idPhysics_Player::IsAirborne() ) {
					idPhysics_Player::TuckLegs();
				}
			}
		} else if ( current.movementFlags & PMF_DUCKED ) {
			// stand up only if there is room, otherwise stay crouched. In the air the legs come down
			// first and the head only goes up by what the floor below doesn't allow
			if ( idPhysics_Player::IsAirborne() ? idPhysics_Player::StandUpInAir() : idPhysics_Player::HasHeadroom() ) {
				current.movementFlags &= ~PMF_DUCKED;
			}
		}

		if ( current.movementFlags & PMF_DUCKED ) {
			playerSpeed = crouchSpeed;
			maxZ = pm_crouchheight.GetFloat();
		} else {
			maxZ = pm_normalheight.GetFloat();
		}
	}
	idPhysics_Player::SetClipHeight( maxZ );
}

/*
================
idPhysics_Player::SetClipHeight
================
*/
void idPhysics_Player::SetClipHeight( const float maxZ ) {
	idBounds bounds;

	// if the clipModel height should change
	if ( clipModel->GetBounds()[1][2] != maxZ ) {

		bounds = clipModel->GetBounds();
		bounds[1][2] = maxZ;
		if ( pm_usecylinder.GetBool() ) {
			clipModel->LoadModel( idTraceModel( bounds, 8 ) );
		} else {
			clipModel->LoadModel( idTraceModel( bounds ) );
		}
	}
}

/*
================
idPhysics_Player::CanUncrouch

Sweeps the crouched box up by the height difference to the standing box.
Anything in the way (low ceiling, vent, table) keeps the player crouched.
================
*/
bool idPhysics_Player::CanUncrouch( void ) const {
	if ( !( current.movementFlags & PMF_DUCKED ) ) {
		return true;
	}
	if ( idPhysics_Player::HasHeadroom() ) {
		return true;
	}
	// in the air the legs can come down instead
	return idPhysics_Player::IsAirborne() && idPhysics_Player::HasFootroom();
}

/*
================
idPhysics_Player::IsAirborne

Regular movement off the ground: crouching here tucks the legs instead of lowering the head.
================
*/
bool idPhysics_Player::IsAirborne( void ) const {
	return current.movementType == PM_NORMAL && !walking && !ladder && waterLevel <= WATERLEVEL_FEET;
}

/*
================
idPhysics_Player::HasFootroom

The crouch sized box can grow to standing height downwards here (in the air).
================
*/
bool idPhysics_Player::HasFootroom( void ) const {
	trace_t	trace;
	const float shift = pm_normalheight.GetFloat() - clipModel->GetBounds()[1][2];

	if ( shift <= 0.0f ) {
		return true;
	}
	gameLocal.clip.Translation( trace, current.origin, current.origin + gravityNormal * shift, clipModel, clipModel->GetAxis(), clipMask, self );
	return ( trace.fraction >= 1.0f );
}

/*
================
idPhysics_Player::TuckLegs

Crouching in the air (crouch jump): the box shrinks from the bottom instead of the top, so the head
stays where it was and the feet go up, clearing higher ledges and getting into vents. The crouched
box ends up inside the space the standing box already had, so this can't put it into anything.
idPlayer moves the eye down by the same amount so the camera doesn't jump.
================
*/
void idPhysics_Player::TuckLegs( void ) {
	const float shift = clipModel->GetBounds()[1][2] - pm_crouchheight.GetFloat();

	if ( shift <= 0.0f ) {
		return;
	}
	current.origin -= gravityNormal * shift;
	duckOriginShift += shift;
}

/*
================
idPhysics_Player::StandUpInAir

Standing up in the air puts the legs down as far as the floor below allows and only raises the head
for the rest, which needs room above it. Returns false (nothing moved) if the standing box doesn't fit.
================
*/
bool idPhysics_Player::StandUpInAir( void ) {
	trace_t	trace;
	idVec3	lowered;
	float	shift, dropped;

	shift = pm_normalheight.GetFloat() - clipModel->GetBounds()[1][2];
	if ( shift <= 0.0f ) {
		return true;
	}

	// legs down
	gameLocal.clip.Translation( trace, current.origin, current.origin + gravityNormal * shift, clipModel, clipModel->GetAxis(), clipMask, self );
	lowered = trace.endpos;
	dropped = ( lowered - current.origin ) * gravityNormal;

	// what the floor didn't allow comes off the top: sweep the crouch box up from the lowered spot,
	// its top then covers exactly the part of the standing box that isn't known to be free yet
	if ( dropped < shift - 0.01f ) {
		gameLocal.clip.Translation( trace, lowered, lowered - gravityNormal * shift, clipModel, clipModel->GetAxis(), clipMask, self );
		if ( trace.fraction < 1.0f ) {
			return false;
		}
	}

	current.origin = lowered;
	duckOriginShift -= dropped;
	return true;
}

/*
================
idPhysics_Player::ConsumeDuckOriginShift

How far crouching / standing up in the air moved the origin up since the last call.
================
*/
float idPhysics_Player::ConsumeDuckOriginShift( void ) {
	const float shift = duckOriginShift;
	duckOriginShift = 0.0f;
	return shift;
}

/*
================
idPhysics_Player::HasHeadroom

The crouch sized box can grow to standing height here.
================
*/
bool idPhysics_Player::HasHeadroom( void ) const {
	trace_t	trace;
	idVec3	end;

	end = current.origin - ( pm_normalheight.GetFloat() - pm_crouchheight.GetFloat() ) * gravityNormal;
	gameLocal.clip.Translation( trace, current.origin, end, clipModel, clipModel->GetAxis(), clipMask, self );

	return ( trace.fraction >= 1.0f );
}

/*
================
idPhysics_Player::CheckSlideStart

A fresh crouch press while sprinting on the ground starts a slide.
================
*/
void idPhysics_Player::CheckSlideStart( void ) {
	idVec3	flatVelocity;
	float	flatSpeed;
	float	speed;
	bool	crouchPressed;
	bool	landed;

	crouchPressed = ( command.upmove < 0 ) && !crouchHeld;
	crouchHeld = ( command.upmove < 0 );
	landed = walking && !wasWalking;
	wasWalking = walking;

	if ( isSliding || !walking || ladder || gameLocal.time < slideCooldownTimer ) {
		return;
	}
	if ( current.movementType != PM_NORMAL || waterLevel > WATERLEVEL_FEET ) {
		return;
	}

	flatVelocity = current.velocity - ( current.velocity * gravityNormal ) * gravityNormal;
	flatSpeed = flatVelocity.Length();

	if ( crouchPressed && isSprinting && !( current.movementFlags & PMF_DUCKED ) &&
			flatSpeed >= walkSpeed * PM_SLIDE_MIN_SPEED_FRAC ) {
		// sprint slide: lock the horizontal view direction and give a burst of speed.
		// walkSpeed holds the current sprint speed set by idPlayer::AdjustSpeed
		slideDir = viewForward - ( viewForward * gravityNormal ) * gravityNormal;
		if ( slideDir.Normalize() < 0.001f ) {
			return;
		}
		speed = Max( flatSpeed, walkSpeed * PM_SLIDE_SPEED_SCALE );
	} else if ( landed && command.upmove < 0 && flatSpeed >= pm_walkspeed.GetFloat() * PM_SLIDE_LAND_SPEED_SCALE ) {
		// landing slide: crouch held while touching down fast turns the fall into a slide.
		// no burst, it just keeps the momentum in the direction we were already moving
		slideDir = flatVelocity / flatSpeed;
		speed = flatSpeed;
	} else {
		return;
	}

	isSliding = true;
	slideTimer = gameLocal.time + PM_SLIDE_DURATION_MSEC;
	current.velocity = slideDir * speed + ( current.velocity * gravityNormal ) * gravityNormal;
}

/*
================
idPhysics_Player::EndSlide
================
*/
void idPhysics_Player::EndSlide( void ) {
	if ( isSliding ) {
		isSliding = false;
		slideCooldownTimer = gameLocal.time + PM_SLIDE_COOLDOWN_MSEC;
	}
}

/*
================
idPhysics_Player::PlayerBounds

Player box with the standing or crouched height.
================
*/
idBounds idPhysics_Player::PlayerBounds( const bool crouched ) const {
	idBounds bounds = clipModel->GetBounds();
	bounds[1][2] = crouched ? pm_crouchheight.GetFloat() : pm_normalheight.GetFloat();
	return bounds;
}

/*
================
IsVaultWall

A near vertical face at most PM_VAULT_MAX_ANGLE_COS away from where we look.
================
*/
static bool IsVaultWall( const trace_t &trace, const idVec3 &forward, const idVec3 &up ) {
	if ( trace.fraction >= 1.0f ) {
		return false;
	}
	if ( idMath::Fabs( trace.c.normal * up ) >= PM_VAULT_MAX_WALL_SLOPE ) {
		return false;
	}
	return ( forward * -trace.c.normal ) >= PM_VAULT_MAX_ANGLE_COS;
}

/*
================
VaultBoxFits

Position test: is the box free of solids at this spot? A (near) zero length sweep can't tell,
the collision code only reports surfaces crossed during the move, not overlaps at the start.
================
*/
static bool VaultBoxFits( const idVec3 &pos, const idBounds &bounds, int mask, const idEntity *pass ) {
	const idTraceModel trm( bounds );
	idClipModel box( trm );

	return ( gameLocal.clip.Contents( pos, &box, mat3_identity, mask, pass ) & mask ) == 0;
}

/*
================
VaultPathClear
================
*/
static bool VaultPathClear( const idVec3 &from, const idVec3 &to, const idBounds &bounds, int mask, const idEntity *pass ) {
	trace_t trace;

	gameLocal.clip.TraceBounds( trace, from, to, bounds, mask, pass );
	return ( trace.fraction >= 1.0f );
}

/*
================
LowVaultPathClear

The arc of a low vault: straight up to arc above the edge, over to the landing spot, down onto it.
================
*/
static bool LowVaultPathClear( const idVec3 &from, const idVec3 &standSpot, const idVec3 &up, float ledgeHeight, float arc,
		const idBounds &bounds, int mask, const idEntity *pass ) {
	const idVec3 peak = from + up * ( ledgeHeight + arc );
	const idVec3 over = standSpot + up * ( arc - PM_VAULT_TARGET_CLEARANCE );

	return VaultPathClear( from, peak, bounds, mask, pass ) &&
		VaultPathClear( peak, over, bounds, mask, pass ) &&
		VaultPathClear( over, standSpot, bounds, mask, pass );
}

/*
================
idPhysics_Player::CheckVaultOpportunity

Finds a climbable obstacle in front of the player with geometry probes, no map triggers needed:
  - waist probe hits and head probe is free: the obstacle top is between the two
  - head probe hits and the probe above the head is free: the ledge top is between those two
The face must be PM_VAULT_MIN_DIST..PM_VAULT_MAX_DIST in front of us and at most 45 degrees
off the view. Then probes down behind the face for a walkable top, checks the player box
fits there (standing or crouched) and that the way there is free for the move that will be
used: the arc of a low vault or the straight pull up of a ledge grab.
A crouched player vaults as is: the probes stay under a low ceiling and the way is checked with
the crouch sized box, so there is no need to stand up first.
outTargetPos is where the player ends up standing: on top, just past the edge. For a low vault
with a carrySpeed it is pushed further forward (see below).
outArc is how far above the edge a low vault passes.
================
*/
static int LowVaultTiming( float speed, bool crouched );

bool idPhysics_Player::CheckVaultOpportunity( trace_t &outWallTrace, idVec3 &outTargetPos, vaultState_t &outType, bool *outCrouch, float probeDist, float *outWallDist, float *outArc, float carrySpeed ) {
	trace_t			waistTrace, headTrace, topTrace, ledgeTrace, ceilingTrace, openingTrace, openingWall;
	const trace_t *	wall;
	idVec3			up, forward, start, flat, ledgeSpot, standSpot, peak;
	idBounds		bounds, pathBounds;
	float			probeTop, ledgeHeight, halfWidth, wallDist, headHeight, topHeight, arc;
	bool			crouch, ducked;
	int				mask;

	up = -gravityNormal;
	forward = viewForward - ( viewForward * gravityNormal ) * gravityNormal;
	if ( forward.Normalize() < 0.001f ) {
		return false;
	}

	halfWidth = clipModel->GetBounds()[1][0];
	mask = clipMask & ~CONTENTS_BODY;		// never climb onto monsters or other players
	ducked = ( current.movementFlags & PMF_DUCKED ) != 0;

	// crouched, the ceiling can be lower than the head probes: a probe that starts inside it never
	// finds the obstacle, so keep them just under whatever is above the player
	headHeight = PM_VAULT_HEAD_HEIGHT;
	topHeight = PM_VAULT_TOP_HEIGHT;
	if ( ducked ) {
		start = current.origin + up * PM_VAULT_CEILING_GAP;
		gameLocal.clip.TracePoint( ceilingTrace, start, current.origin + up * PM_VAULT_TOP_HEIGHT, mask, self );
		if ( ceilingTrace.fraction < 1.0f ) {
			const float ceiling = ( ceilingTrace.endpos - current.origin ) * up;
			topHeight = ceiling - PM_VAULT_CEILING_GAP;
			headHeight = Min( headHeight, topHeight );
			if ( headHeight <= PM_VAULT_WAIST_HEIGHT ) {
				return false;
			}
		}
	}

	// 1. waist and head probes
	start = current.origin + up * PM_VAULT_WAIST_HEIGHT;
	gameLocal.clip.TracePoint( waistTrace, start, start + forward * probeDist, mask, self );
	start = current.origin + up * headHeight;
	gameLocal.clip.TracePoint( headTrace, start, start + forward * probeDist, mask, self );

	// 2. classify
	wall = NULL;
	probeTop = 0.0f;
	if ( IsVaultWall( waistTrace, forward, up ) && headTrace.fraction >= 1.0f ) {
		wall = &waistTrace;
		probeTop = headHeight;
	} else if ( IsVaultWall( headTrace, forward, up ) && topHeight > headHeight ) {
		// high ledge: there must be room above the head
		start = current.origin + up * topHeight;
		gameLocal.clip.TracePoint( topTrace, start, start + forward * probeDist, mask, self );
		if ( topTrace.fraction >= 1.0f ) {
			wall = &headTrace;
			probeTop = topHeight;
		}
	}
	if ( wall == NULL ) {
		// a wall at the waist that goes on above the head: look for an opening in it (vent, duct,
		// window). The lowest free probe is the opening, the face is the last hit just below it
		if ( !IsVaultWall( waistTrace, forward, up ) ) {
			return false;
		}
		openingWall = waistTrace;
		for ( float h = PM_VAULT_WAIST_HEIGHT + PM_VAULT_OPENING_STEP; h <= topHeight; h += PM_VAULT_OPENING_STEP ) {
			start = current.origin + up * h;
			gameLocal.clip.TracePoint( openingTrace, start, start + forward * probeDist, mask, self );
			if ( openingTrace.fraction >= 1.0f ) {
				wall = &openingWall;
				probeTop = h;
				break;
			}
			if ( !IsVaultWall( openingTrace, forward, up ) ) {
				return false;
			}
			openingWall = openingTrace;
		}
		if ( wall == NULL ) {
			return false;
		}
	}

	wallDist = wall->fraction * probeDist;
	if ( wallDist < halfWidth - 1.0f ) {
		// can't be inside the obstacle, the per-type ranges are checked by CheckVaultStart
		return false;
	}
	if ( outWallDist ) {
		*outWallDist = wallDist;
	}

	// 3. find the top just behind the face
	flat = wall->endpos - current.origin;
	flat -= ( flat * up ) * up;
	ledgeSpot = current.origin + flat + forward * PM_VAULT_LEDGE_INSET;
	gameLocal.clip.TracePoint( ledgeTrace, ledgeSpot + up * probeTop, ledgeSpot + up * ( maxStepHeight + 1.0f ), mask, self );
	if ( ledgeTrace.fraction <= 0.0f || ledgeTrace.fraction >= 1.0f || ( ledgeTrace.c.normal * up ) < PM_VAULT_MIN_LEDGE_NORMAL ) {
		return false;
	}
	ledgeHeight = ( ledgeTrace.endpos - current.origin ) * up;
	if ( ledgeHeight < PM_VAULT_MIN_HEIGHT ) {
		return false;
	}
	outType = ( ledgeHeight <= PM_VAULT_LOW_MAX_HEIGHT ) ? VAULT_LOW : VAULT_HIGH_GRAB;

	// 4. the box must fit on top, just past the edge, with PM_VAULT_FIT_MARGIN to spare: standing, or at
	// least crouched. Checked before anything moves, a spot that doesn't fit cancels the vault
	standSpot = current.origin + flat + forward * ( halfWidth + PM_VAULT_LAND_INSET ) + up * ( ledgeHeight + PM_VAULT_TARGET_CLEARANCE );
	crouch = ducked;
	if ( crouch || !VaultBoxFits( standSpot, PlayerBounds( false ).Expand( PM_VAULT_FIT_MARGIN ), clipMask, self ) ) {
		crouch = true;
		if ( !VaultBoxFits( standSpot, PlayerBounds( true ).Expand( PM_VAULT_FIT_MARGIN ), clipMask, self ) ) {
			return false;
		}
	}

	// 5. the way there must be free, checked with the box the move will really use, so the camera (which
	// stays inside that box) can't be taken into a ceiling either
	arc = PM_VAULT_LOW_ARC;
	if ( outType == VAULT_LOW ) {
		// arc that peaks PM_VAULT_LOW_ARC above the edge, or a flat pass that only keeps the landing
		// clearance when that hits the ceiling. Standing if there is room for it, otherwise crouched
		bool found = false;
		for ( int crouched = crouch ? 1 : 0; crouched < 2 && !found; crouched++ ) {
			pathBounds = PlayerBounds( crouched != 0 );
			for ( int flatArc = 0; flatArc < 2 && !found; flatArc++ ) {
				arc = flatArc ? PM_VAULT_TARGET_CLEARANCE : PM_VAULT_LOW_ARC;
				if ( LowVaultPathClear( current.origin, standSpot, up, ledgeHeight, arc, pathBounds, clipMask, self ) ) {
					found = true;
					crouch = ( crouched != 0 );
				}
			}
		}
		if ( !found ) {
			return false;
		}

		// stretch the landing forward so the pass covers what the run-up speed would in the same time:
		// the vault crosses at the entry speed (a glide) instead of braking on top or dashing over. The
		// spot backs off towards the edge while the box doesn't fit or the way there isn't free
		if ( carrySpeed > 0.0f ) {
			idVec3 nearFlat = standSpot - current.origin;
			nearFlat -= ( nearFlat * up ) * up;
			const float extend = Min( carrySpeed * MS2SEC( LowVaultTiming( carrySpeed, crouch ) ) - nearFlat.Length(), PM_VAULT_LAND_EXTEND_MAX );
			pathBounds = PlayerBounds( crouch );
			const idBounds fitBounds = pathBounds.Expand( PM_VAULT_FIT_MARGIN );
			for ( float ext = extend; ext > 0.5f; ext -= PM_VAULT_LAND_EXTEND_STEP ) {
				const idVec3 spot = standSpot + forward * ext;
				if ( VaultBoxFits( spot, fitBounds, clipMask, self ) && LowVaultPathClear( current.origin, spot, up, ledgeHeight, arc, pathBounds, clipMask, self ) ) {
					standSpot = spot;
					break;
				}
			}
		}
	} else {
		// ledge grabs pull straight up along the wall, then over the edge. A crouched player keeps
		// the crouch sized box for the whole climb
		pathBounds = PlayerBounds( crouch );
		peak = current.origin + up * ( ledgeHeight + PM_VAULT_TARGET_CLEARANCE );
		if ( !VaultPathClear( current.origin, peak, pathBounds, clipMask, self ) ||
			!VaultPathClear( peak, standSpot, pathBounds, clipMask, self ) ) {
			return false;
		}
	}

	outWallTrace = *wall;
	outTargetPos = standSpot;
	if ( outCrouch ) {
		*outCrouch = crouch;
	}
	if ( outArc ) {
		*outArc = arc;
	}
	return true;
}

/*
================
VaultDuration

Linear from minMsec at minHeight to maxMsec at maxHeight, clamped.
================
*/
static int VaultDuration( float height, float minHeight, float maxHeight, int minMsec, int maxMsec ) {
	const float f = idMath::ClampFloat( 0.0f, 1.0f, ( height - minHeight ) / Max( maxHeight - minHeight, 1.0f ) );
	return minMsec + idMath::Ftoi( f * ( maxMsec - minMsec ) );
}

/*
================
LowVaultTiming

Interpolates the low vault duration between crouched/slow, walking, and sprinting movement.
================
*/
static int LowVaultTiming( float speed, bool crouched ) {
	if ( crouched || speed <= PM_VAULT_LOW_SLOW_SPEED ) {
		return PM_VAULT_LOW_CROUCH_MSEC;
	}
	if ( speed < PM_VAULT_LOW_WALK_SPEED ) {
		const float fraction = idMath::ClampFloat( 0.0f, 1.0f,
			( speed - PM_VAULT_LOW_SLOW_SPEED ) / ( PM_VAULT_LOW_WALK_SPEED - PM_VAULT_LOW_SLOW_SPEED ) );
		return PM_VAULT_LOW_CROUCH_MSEC + idMath::Ftoi( fraction * ( PM_VAULT_LOW_WALK_MSEC - PM_VAULT_LOW_CROUCH_MSEC ) );
	}
	if ( speed < PM_VAULT_LOW_SPRINT_SPEED ) {
		const float fraction = idMath::ClampFloat( 0.0f, 1.0f,
			( speed - PM_VAULT_LOW_WALK_SPEED ) / ( PM_VAULT_LOW_SPRINT_SPEED - PM_VAULT_LOW_WALK_SPEED ) );
		return PM_VAULT_LOW_WALK_MSEC + idMath::Ftoi( fraction * ( PM_VAULT_LOW_SPRINT_MSEC - PM_VAULT_LOW_WALK_MSEC ) );
	}
	return PM_VAULT_LOW_SPRINT_MSEC;
}

/*
================
LowVaultBezierControl

Control point of the low vault's quadratic Bezier P(u) = (1-u)^2 S + 2(1-u)u C + u^2 E, as
( along, up ) offsets from S. E is ( dist, height ).

Up: the curve peaks at c^2 / ( 2c - H ), solving that for a peak of H + arc gives
c = P + sqrt( P * arc ) with P = H + arc. The peak comes after the middle of the pass, so the
body is still rising when it reaches the edge and only settles at the very end.

Along: C at dist / 2 makes the horizontal motion linear. It is pulled back when that would make
the front of the box reach the face (approachDist) before the bottom is above the edge
(edgeHeight). Standing right against the face that needs C behind the start, so the body draws
back a little while it rises (at most PM_VAULT_BEZIER_MAX_DIP). Only the highest obstacles taken
from right against the face, or the flat arc under a low ceiling, need more: VaultSweep guards
whatever is left.
================
*/
static void LowVaultBezierControl( float dist, float height, float edgeHeight, float arc, float approachDist,
		float &outAlong, float &outUp ) {
	const float peak = height + arc;
	const float c = peak + idMath::Sqrt( peak * Max( arc, 0.0f ) );
	outUp = c;

	// first u where the bottom of the box is level with the edge: ( H - 2c ) u^2 + 2c u - edge = 0
	const float k = 2.0f * c - height;
	const float disc = 4.0f * c * c - 4.0f * k * edgeHeight;
	if ( k <= 0.0f || disc < 0.0f ) {
		outAlong = 0.5f * dist;
		return;
	}
	const float uClear = ( 2.0f * c - idMath::Sqrt( disc ) ) / ( 2.0f * k );

	// x( uClear ) = 2 uClear ( 1 - uClear ) along + uClear^2 dist must not pass approachDist
	float along = 0.5f * dist;
	const float b = 2.0f * uClear * ( 1.0f - uClear );
	if ( b > 0.0001f ) {
		along = Min( along, ( approachDist - uClear * uClear * dist ) / b );
	}
	// the lowest point of x( u ) for along < 0 is -along^2 / ( dist - 2 along ): keep it within the max dip
	const float m = PM_VAULT_BEZIER_MAX_DIP;
	const float minAlong = -m - idMath::Sqrt( m * m + m * dist );
	outAlong = idMath::ClampFloat( minAlong, 0.5f * dist, along );
}

/*
================
idPhysics_Player::CheckVaultStart

A fresh jump press is remembered for PM_VAULT_INPUT_BUFFER_MSEC: pressing jump a bit before
reaching the obstacle, or while already in front of it, starts a vault. Holding jump does not.
================
*/
bool idPhysics_Player::CheckVaultStart( void ) {
	trace_t			wallTrace, startTrace;
	idVec3			standSpot, up, flatVelocity, flat;
	vaultState_t	type;
	float			flatDist, wallFaceDist, halfWidth, probeDist, wallDist, arc;
	bool			crouch;

	if ( vaultInputBuffer <= gameLocal.time ) {
		return false;
	}
	if ( current.movementType != PM_NORMAL || ladder || waterLevel > WATERLEVEL_FEET || masterEntity ) {
		return false;
	}
	if ( gameLocal.time < vaultCooldownTimer || command.forwardmove < 0 ) {
		return false;
	}

	// the kinematic move must start from a valid spot: not with the box overlapping something (a seam of
	// overlapping brushes is tolerated, the box is nudged back out of it), and not pressed against a
	// ceiling, where it couldn't rise anyway
	flat = viewForward - ( viewForward * gravityNormal ) * gravityNormal;
	if ( flat.Normalize() < 0.001f || !idPhysics_Player::NudgeOutOfSolid( -flat ) ) {
		return false;
	}
	gameLocal.clip.Translation( startTrace, current.origin, current.origin - gravityNormal * PM_VAULT_START_HEADROOM, clipModel, clipModel->GetAxis(), clipMask, self );
	if ( startTrace.fraction < 1.0f ) {
		return false;
	}

	flatVelocity = current.velocity - ( current.velocity * gravityNormal ) * gravityNormal;

	// on the ground, also look as far ahead as we will get while the press is still buffered
	probeDist = PM_VAULT_MAX_DIST;
	if ( walking ) {
		probeDist += flatVelocity.Length() * MS2SEC( vaultInputBuffer - gameLocal.time );
	}
	if ( !idPhysics_Player::CheckVaultOpportunity( wallTrace, standSpot, type, &crouch, probeDist, &wallDist, &arc, flatVelocity.Length() ) ) {
		return false;
	}
	up = -gravityNormal;
	halfWidth = clipModel->GetBounds()[1][0];

	// allowed range for this kind of vault, as the gap between the front of the player box and the face
	const float gap = wallDist - halfWidth;
	const float minGap = ( type == VAULT_LOW ) ? PM_VAULT_LOW_MIN_DIST : PM_VAULT_MIN_DIST - halfWidth;
	const float maxGap = ( type == VAULT_LOW ) ? PM_VAULT_LOW_MAX_DIST : PM_VAULT_MAX_DIST - halfWidth;
	const float approachSpeed = Max( 0.0f, flatVelocity * ( -wallTrace.c.normal ) );

	if ( gap < minGap - 1.0f ) {
		// too close
		return false;
	}
	if ( gap > maxGap ) {
		// pressed a bit early: if we will be in range before the buffered press runs out, hold back the
		// regular jump so the press becomes a vault once in range. Otherwise let the jump happen
		if ( walking && gap - approachSpeed * MS2SEC( vaultInputBuffer - gameLocal.time ) <= maxGap ) {
			current.movementFlags |= PMF_JUMP_HELD;
		}
		return false;
	}

	if ( pm_vaultDebug.GetBool() ) {
		gameLocal.Printf( "vault: %s ledge %.1f gap %.1f crouch %d ducked %d arc %.0f speed %.0f %s\n", type == VAULT_LOW ? "LOW" : "HIGH",
			( standSpot - current.origin ) * up - PM_VAULT_TARGET_CLEARANCE, gap, crouch, ( current.movementFlags & PMF_DUCKED ) != 0,
			arc, flatVelocity.Length(), walking ? "ground" : "air" );
	}

	// the vault clock starts at the beginning of this physics step, so the first frame already moves
	vaultType = type;
	vaultStartTime = gameLocal.time - framemsec;
	vaultPhaseStartTime = vaultStartTime;
	vaultStartPos = current.origin;
	vaultPhaseStartPos = current.origin;
	vaultLedgeNormal = wallTrace.c.normal;
	vaultEntryVelocity = flatVelocity;
	vaultLedgeHeight = ( standSpot - current.origin ) * up;
	flat = ( standSpot - current.origin ) - up * vaultLedgeHeight;
	flatDist = flat.Normalize();
	vaultForward = flat;
	vaultWallContact = false;
	vaultArc = arc;
	// started crouched, or only room for a crouched player on top: the box stays crouch sized for
	// the whole move (CheckDuck doesn't run while vaulting) and the camera is kept level
	vaultCrouched = crouch || ( current.movementFlags & PMF_DUCKED );

	if ( type == VAULT_LOW ) {
		// one continuous pass whose duration follows takeoff speed; XY and the arc share the clock
		// and finish together. The run-up speed comes back when the pass ends
		const float edgeHeight = vaultLedgeHeight - PM_VAULT_TARGET_CLEARANCE;
		const int passMsec = LowVaultTiming( flatVelocity.Length(), vaultCrouched );
		// the landing may have been stretched forward, the face is where the probe found it
		const float approachDist = Max( 0.0f, wallDist - halfWidth );
		float controlAlong, controlUp;
		LowVaultBezierControl( flatDist, vaultLedgeHeight, edgeHeight, vaultArc, approachDist, controlAlong, controlUp );
		vaultControlPos = current.origin + vaultForward * controlAlong + up * controlUp;
		vaultMoveSpeed = flatDist / MS2SEC( passMsec );
		vaultTargetPos = standSpot;
		vaultTimer = vaultStartTime + passMsec;
		currentVaultState = VAULT_LOW;
		if ( crouch ) {
			// the pass or the landing spot only fits crouched: duck for real so the eye goes down with
			// the box. CheckDuck stands back up after the vault if crouch isn't held and there is room
			current.movementFlags |= PMF_DUCKED;
			idPhysics_Player::SetClipHeight( pm_crouchheight.GetFloat() );
		}
	} else {
		// the kinematic part ends with the front of the box PM_VAULT_MANTLE_OVERLAP past the edge,
		// which is enough for the ground check to catch the ledge; walking takes it from there
		wallFaceDist = flatDist - halfWidth - PM_VAULT_LAND_INSET;
		vaultTargetPos = current.origin + vaultForward * ( wallFaceDist + PM_VAULT_MANTLE_OVERLAP - halfWidth ) + up * vaultLedgeHeight;
		vaultAbsorbSpeed = Max( 0.0f, flatVelocity * vaultForward );
		vaultAbsorbMax = Max( 0.0f, wallFaceDist - halfWidth - 0.5f );
		vaultMoveSpeed = flatVelocity.Length();
		// total duration by ledge height only, split between the three phases
		const int totalMsec = VaultDuration( vaultLedgeHeight - PM_VAULT_TARGET_CLEARANCE, PM_VAULT_LOW_MAX_HEIGHT, PM_VAULT_TOP_HEIGHT,
			PM_VAULT_HIGH_DURATION_MSEC, PM_VAULT_HIGH_DURATION_MAX_MSEC );
		const int absorbMsec = Max( 1, idMath::Ftoi( totalMsec * PM_VAULT_ABSORB_FRAC ) );
		vaultMantleMsec = Max( 1, idMath::Ftoi( totalMsec * PM_VAULT_MANTLE_FRAC ) );
		vaultRiseMsec = Max( 1, totalMsec - absorbMsec - vaultMantleMsec );
		vaultTimer = vaultStartTime + absorbMsec;
		currentVaultState = VAULT_HIGH_GRAB;
		if ( crouch ) {
			// only room for a crouched player up there
			current.movementFlags |= PMF_DUCKED;
			idPhysics_Player::SetClipHeight( pm_crouchheight.GetFloat() );
		}
	}

	// the jump press is used up by the vault: latched until jump is released and the vault is over
	vaultInputBuffer = 0;
	vaultLatch = true;
	current.movementFlags |= PMF_JUMP_HELD;
	coyoteTimer = 0;
	jumpBufferTimer = 0;
	idPhysics_Player::EndSlide();
	walking = false;
	groundPlane = false;

	return true;
}

/*
================
idPhysics_Player::ProcessVault

VAULT_LOW:       one continuous quadratic Bezier pass over the obstacle, duration by run-up speed.
VAULT_HIGH_GRAB: phase 1, the hands hit the ledge and the run-up speed dies off exponentially.
VAULT_CLIMBING:  phase 2, pull up with EaseOutCubic: fast start, soft arrival above the edge.
VAULT_MANTLE:    phase 3, move over the edge accelerating from 0 back to the run-up speed.
Each step is swept with the player box (VaultSweep) so it can never end up inside geometry.
================
*/
void idPhysics_Player::ProcessVault( int msec ) {
	idVec3	up, oldOrigin, desired, toTarget;
	float	u, s;

	up = -gravityNormal;
	oldOrigin = current.origin;

	// ledge jump: a fresh jump press while hanging or pulling up
	if ( ( currentVaultState == VAULT_HIGH_GRAB || currentVaultState == VAULT_CLIMBING ) && vaultJumpEdge &&
			gameLocal.time >= vaultStartTime + PM_LEDGE_JUMP_MIN_MSEC ) {
		idPhysics_Player::EndVault( false );
		current.velocity = up * PM_LEDGE_JUMP_SPEED + vaultForward * PM_LEDGE_JUMP_FORWARD;
		current.movementFlags |= PMF_JUMP_HELD | PMF_JUMPED;
		return;
	}

	// advance through the ledge grab phases, each one starts exactly where the last one ended in time
	while ( vaultType == VAULT_HIGH_GRAB && currentVaultState != VAULT_MANTLE && gameLocal.time >= vaultTimer ) {
		vaultPhaseStartTime = vaultTimer;
		vaultPhaseStartPos = current.origin;
		if ( currentVaultState == VAULT_HIGH_GRAB ) {
			currentVaultState = VAULT_CLIMBING;
			vaultTimer += vaultRiseMsec;
		} else {
			currentVaultState = VAULT_MANTLE;
			// EaseInQuad x = D * u^2 ends at 2D / T, which is the speed handed back at the end. T is not
			// shortened for fast run-ups: the climb takes the same time whatever the approach was
			toTarget = vaultTargetPos - current.origin;
			toTarget -= ( toTarget * up ) * up;
			const float dist = toTarget.Length();
			vaultMoveSpeed = 2.0f * dist / MS2SEC( vaultMantleMsec );
			vaultTimer += vaultMantleMsec;
		}
	}

	u = idMath::ClampFloat( 0.0f, 1.0f, ( gameLocal.time - vaultPhaseStartTime ) / (float)( vaultTimer - vaultPhaseStartTime ) );

	switch ( currentVaultState ) {
		case VAULT_LOW: {
			// quadratic Bezier start -> control -> landing spot (see LowVaultBezierControl): peaks vaultArc
			// over the edge, never reaches the face before clearing it, lands exactly at T
			s = 1.0f - u;
			desired = vaultStartPos * ( s * s ) + vaultControlPos * ( 2.0f * s * u ) + vaultTargetPos * ( u * u );
			break;
		}
		case VAULT_HIGH_GRAB: {
			// logarithmic braking: travel = v0 * tau * ( 1 - e^(-t/tau) ), never past touching the wall
			const float t = MS2SEC( gameLocal.time - vaultPhaseStartTime );
			const float travel = Min( vaultAbsorbSpeed * PM_VAULT_ABSORB_TAU * ( 1.0f - idMath::Exp( -t / PM_VAULT_ABSORB_TAU ) ), vaultAbsorbMax );
			desired = vaultWallContact ? current.origin : vaultStartPos + vaultForward * travel;
			break;
		}
		case VAULT_CLIMBING: {
			// EaseOutCubic up to the ledge; the box also closes whatever gap is left to the wall, so
			// phase 3 only has to cover PM_VAULT_MANTLE_OVERLAP
			s = 1.0f - ( 1.0f - u ) * ( 1.0f - u ) * ( 1.0f - u );
			const idVec3 contact = vaultStartPos + vaultForward * vaultAbsorbMax;
			idVec3 gap = contact - vaultPhaseStartPos;
			gap -= ( gap * up ) * up;
			desired = vaultPhaseStartPos + gap * s + up * ( ( ( vaultTargetPos - vaultPhaseStartPos ) * up ) * s );
			if ( vaultWallContact ) {
				// already against the wall: only go up
				desired = current.origin + up * ( ( desired - current.origin ) * up );
			}
			break;
		}
		case VAULT_MANTLE: {
			// EaseInQuad: starts at rest on the edge and accelerates to vaultMoveSpeed, which is handed to
			// the regular physics at the end without a jump in velocity
			toTarget = vaultTargetPos - vaultPhaseStartPos;
			toTarget -= ( toTarget * up ) * up;
			const float dist = toTarget.Normalize();
			desired = vaultPhaseStartPos + toTarget * ( dist * u * u );
			desired += up * ( ( vaultTargetPos - desired ) * up );
			break;
		}
		default:
			idPhysics_Player::EndVault( false );
			return;
	}

	// sweep the box there
	const bool onWall = ( currentVaultState == VAULT_HIGH_GRAB || currentVaultState == VAULT_CLIMBING );
	if ( idPhysics_Player::VaultSweep( desired ) && onWall ) {
		// the hands are on the wall: drop the rest of the horizontal move instead of catching up later
		vaultWallContact = true;
	}

	current.localOrigin = current.origin;
	clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() );

	// kinematic velocity, keeps view bob / crash land / network code sane
	if ( frametime > 0.0f ) {
		current.velocity = ( current.origin - oldOrigin ) / frametime;
	}

	// done?
	if ( currentVaultState == VAULT_LOW ) {
		// arrived: this step reached (or passed) the landing spot
		toTarget = vaultTargetPos - current.origin;
		toTarget -= ( toTarget * up ) * up;
		if ( ( toTarget * vaultForward ) <= 0.01f || gameLocal.time >= vaultTimer + PM_VAULT_OVERTIME_MSEC ) {
			idPhysics_Player::EndVault( true );
		}
	} else if ( currentVaultState == VAULT_MANTLE && u >= 1.0f ) {
		idPhysics_Player::EndVault( true );
	}
}

/*
================
idPhysics_Player::NudgeOutOfSolid

Overlapping brushes (imperfect map geometry) can make the box test as overlapping solid at a spot
it reached with regular, swept movement. Instead of treating that as stuck, push it back along away
(out of the obstacle face) by up to PM_VAULT_NUDGE_MAX, to the first spot where it is clean.
Returns false if there is no such spot: the box is really stuck and nothing is moved.
================
*/
bool idPhysics_Player::NudgeOutOfSolid( const idVec3 &away ) {
	if ( !( gameLocal.clip.Contents( current.origin, clipModel, clipModel->GetAxis(), clipMask, self ) & clipMask ) ) {
		return true;
	}
	for ( float d = 0.5f; d <= PM_VAULT_NUDGE_MAX; d += 0.5f ) {
		const idVec3 pos = current.origin + away * d;
		if ( !( gameLocal.clip.Contents( pos, clipModel, clipModel->GetAxis(), clipMask, self ) & clipMask ) ) {
			current.origin = pos;
			return true;
		}
	}
	return false;
}

/*
================
idPhysics_Player::VaultSweep

Moves the box towards desired in substeps of at most PM_VAULT_SWEEP_STEP, each one swept with the
real clip model (continuous collision), so a fast frame can't cut a corner and the box never ends
up inside solid geometry. A blocked substep keeps the vertical part first (up and over the edge),
then whatever horizontal part still fits along the planned direction. Nothing is ever deflected
sideways: what doesn't fit is dropped, never forced or slid. Returns true if anything blocked.
================
*/
bool idPhysics_Player::VaultSweep( const idVec3 &desired ) {
	trace_t	trace;
	idVec3	up, start, step, target, horizontal;
	bool	blocked;
	int		i, numSteps;

	up = -gravityNormal;

	// a seam of overlapping brushes would make every sweep start "in solid" and freeze the vault
	idPhysics_Player::NudgeOutOfSolid( -vaultForward );

	start = current.origin;
	numSteps = Max( 1, idMath::Ftoi( idMath::Ceil( ( desired - start ).Length() / PM_VAULT_SWEEP_STEP ) ) );
	step = ( desired - start ) / (float)numSteps;
	blocked = false;

	for ( i = 1; i <= numSteps; i++ ) {
		// targets stay on the planned path, a substep that was held back catches up with the next ones
		target = start + step * (float)i;

		gameLocal.clip.Translation( trace, current.origin, target, clipModel, clipModel->GetAxis(), clipMask, self );
		if ( trace.fraction >= 1.0f ) {
			current.origin = trace.endpos;
			continue;
		}
		blocked = true;

		// vertical part first
		gameLocal.clip.Translation( trace, current.origin, current.origin + up * ( ( target - current.origin ) * up ), clipModel, clipModel->GetAxis(), clipMask, self );
		current.origin = trace.endpos;

		// then the horizontal part from there
		horizontal = target - current.origin;
		horizontal -= ( horizontal * up ) * up;
		if ( horizontal.LengthSqr() < Square( 0.01f ) ) {
			continue;
		}
		gameLocal.clip.Translation( trace, current.origin, current.origin + horizontal, clipModel, clipModel->GetAxis(), clipMask, self );
		current.origin = trace.endpos;
	}

	return blocked;
}

/*
================
idPhysics_Player::EndVault
================
*/
void idPhysics_Player::EndVault( const bool keepMomentum ) {
	if ( currentVaultState == VAULT_NONE ) {
		return;
	}
	currentVaultState = VAULT_NONE;
	vaultCooldownTimer = gameLocal.time + PM_VAULT_COOLDOWN_MSEC;

	// no new jump or vault until the jump button is released
	vaultLatch = true;
	vaultInputBuffer = 0;
	jumpBufferTimer = 0;

	if ( keepMomentum ) {
		if ( vaultType == VAULT_LOW ) {
			// low vaults give back the run-up velocity with a small push off the obstacle, capped so
			// chained vaults can't build speed beyond the boosted run speed
			const float entrySpeed = vaultEntryVelocity.Length();
			const float boostedSpeed = Min( entrySpeed * PM_VAULT_EXIT_BOOST, Max( entrySpeed, pm_runspeed.GetFloat() * PM_VAULT_EXIT_BOOST ) );
			current.velocity = entrySpeed > 0.001f ? vaultEntryVelocity * ( boostedSpeed / entrySpeed ) : vaultEntryVelocity;
			// and keep it through the landing: no ground friction for the first few grounded frames
			vaultGraceFrames = PM_VAULT_GRACE_FRAMES;
			vaultGraceExpire = gameLocal.time + PM_VAULT_GRACE_MAX_MSEC;
		} else {
			// ledge grabs end at the speed the mantle curve reached (the run-up speed if it was faster)
			current.velocity = vaultForward * vaultMoveSpeed;
		}
	}
}

/*
================
idPhysics_Player::GetVaultProgress

Progress of the current vault phase, 0..1.
================
*/
float idPhysics_Player::GetVaultProgress( void ) const {
	if ( currentVaultState == VAULT_NONE || vaultTimer <= vaultPhaseStartTime ) {
		return 0.0f;
	}
	return idMath::ClampFloat( 0.0f, 1.0f, ( gameLocal.time - vaultPhaseStartTime ) / (float)( vaultTimer - vaultPhaseStartTime ) );
}

/*
================
idPhysics_Player::GetVaultDurationMsec

Total duration of the current or most recently completed vault.
================
*/
int idPhysics_Player::GetVaultDurationMsec( void ) const {
	int duration = vaultTimer - vaultStartTime;
	if ( vaultType == VAULT_HIGH_GRAB && currentVaultState == VAULT_HIGH_GRAB ) {
		duration += vaultRiseMsec + vaultMantleMsec;
	} else if ( vaultType == VAULT_HIGH_GRAB && currentVaultState == VAULT_CLIMBING ) {
		duration += vaultMantleMsec;
	}
	return Max( 0, duration );
}

/*
================
idPhysics_Player::ProcessSlide

Ground movement while sliding: low friction, mostly locked direction, ramps accelerate.
================
*/
void idPhysics_Player::ProcessSlide( int msec ) {
	idVec3	flatView, slideRight, slopeGravity, oldVelocity;
	float	speed, slopeAccel, oldVel, newVel;
	float	dt = MS2SEC( msec );

	// jump cancels the slide, CheckJump keeps the horizontal velocity so the momentum carries into the jump
	if ( ( ( command.upmove >= 10 && !( current.movementFlags & PMF_JUMP_HELD ) ) || jumpBufferTimer > gameLocal.time ) && CanUncrouch() ) {
		idPhysics_Player::EndSlide();
		current.movementFlags &= ~PMF_DUCKED;
		idPhysics_Player::SetClipHeight( pm_normalheight.GetFloat() );
		if ( idPhysics_Player::CheckJump() ) {
			idPhysics_Player::AirMove();
			return;
		}
	}

	// speed along the locked direction, sideways speed from steering is not carried over
	speed = ( current.velocity - ( current.velocity * gravityNormal ) * gravityNormal ) * slideDir;
	if ( speed < 0.0f ) {
		speed = 0.0f;
	}

	// low friction
	speed -= speed * PM_SLIDE_FRICTION * dt;

	// gravity along the ground plane: speeds up downhill, slows down uphill
	slopeGravity = gravityVector - ( gravityVector * groundTrace.c.normal ) * groundTrace.c.normal;
	slopeAccel = slopeGravity * slideDir;
	speed += slopeAccel * PM_SLIDE_SLOPE_SCALE * dt;
	speed = idMath::ClampFloat( 0.0f, PM_SLIDE_MAX_SPEED, speed );

	// a steep enough ramp keeps the slide alive indefinitely
	if ( slopeAccel > PM_SLIDE_MIN_SLOPE_ACCEL ) {
		slideTimer = Max( slideTimer, gameLocal.time + PM_SLIDE_SLOPE_GRACE_MSEC );
	}

	// let slideDir slowly follow the camera
	flatView = viewForward - ( viewForward * gravityNormal ) * gravityNormal;
	if ( flatView.Normalize() > 0.001f ) {
		float cosAngle = idMath::ClampFloat( -1.0f, 1.0f, slideDir * flatView );
		float angle = RAD2DEG( idMath::ACos( cosAngle ) );
		float maxTurn = PM_SLIDE_TURN_RATE * dt;
		if ( angle > 0.01f ) {
			float turn = Min( angle, maxTurn );
			// rotate around the up axis towards the view
			float sign = ( ( slideDir.Cross( flatView ) ) * -gravityNormal ) >= 0.0f ? 1.0f : -1.0f;
			idRotation rotation( vec3_origin, -gravityNormal, sign * turn );
			slideDir *= rotation;
			slideDir.Normalize();
		}
	}

	// end of slide: time is up or we slowed down to walking speed, keep moving this frame
	if ( gameLocal.time >= slideTimer || speed < pm_walkspeed.GetFloat() ) {
		idPhysics_Player::EndSlide();
	}

	// a little sideways control from the strafe keys
	slideRight = gravityNormal.Cross( slideDir );
	current.velocity = slideDir * speed + slideRight * ( PM_SLIDE_STEER_SPEED * command.rightmove / 127.0f );

	// follow the ground plane without losing speed on slopes
	oldVelocity = current.velocity;
	current.velocity.ProjectOntoPlane( groundTrace.c.normal, OVERCLIP );
	newVel = current.velocity.LengthSqr();
	oldVel = oldVelocity.LengthSqr();
	if ( newVel > 1.0f && oldVel > 1.0f ) {
		current.velocity *= idMath::Sqrt( oldVel / newVel );
	}

	if ( !current.velocity.LengthSqr() ) {
		return;
	}

	gameLocal.push.InitSavingPushedEntityPositions();

	idPhysics_Player::SlideMove( false, true, true, true );
}

/*
================
idPhysics_Player::CheckLadder
================
*/
void idPhysics_Player::CheckLadder( void ) {
	idVec3		forward, start, end;
	trace_t		trace;
	float		tracedist;

	if ( current.movementTime ) {
		return;
	}

	// if on the ground moving backwards
	if ( walking && command.forwardmove <= 0 ) {
		return;
	}

	// forward vector orthogonal to gravity
	forward = viewForward - (gravityNormal * viewForward) * gravityNormal;
	forward.Normalize();

	if ( walking ) {
		// don't want to get sucked towards the ladder when still walking
		tracedist = 1.0f;
	} else {
		tracedist = 48.0f;
	}

	end = current.origin + tracedist * forward;
	gameLocal.clip.Translation( trace, current.origin, end, clipModel, clipModel->GetAxis(), clipMask, self );

	// if near a surface
	if ( trace.fraction < 1.0f ) {

		// if a ladder surface
		if ( trace.c.material && ( trace.c.material->GetSurfaceFlags() & SURF_LADDER ) ) {

			// check a step height higher
			end = current.origin - gravityNormal * ( maxStepHeight * 0.75f );
			gameLocal.clip.Translation( trace, current.origin, end, clipModel, clipModel->GetAxis(), clipMask, self );
			start = trace.endpos;
			end = start + tracedist * forward;
			gameLocal.clip.Translation( trace, start, end, clipModel, clipModel->GetAxis(), clipMask, self );

			// if also near a surface a step height higher
			if ( trace.fraction < 1.0f ) {

				// if it also is a ladder surface
				if ( trace.c.material && trace.c.material->GetSurfaceFlags() & SURF_LADDER ) {
					ladder = true;
					ladderNormal = trace.c.normal;
				}
			}
		}
	}
}

/*
=============
idPhysics_Player::CheckJump
=============
*/
bool idPhysics_Player::CheckJump( void ) {
	idVec3 addVelocity;
	bool pressed, buffered;

	// fresh press of the jump button (must wait for jump to be released)
	pressed = ( command.upmove >= 10 ) && !( current.movementFlags & PMF_JUMP_HELD );
	// jump pressed shortly before touching the ground
	buffered = ( jumpBufferTimer > gameLocal.time );

	if ( !pressed && !buffered ) {
		return false;
	}

	// must be on the ground or inside the coyote time window
	if ( !walking && coyoteTimer <= gameLocal.time ) {
		return false;
	}

	// still crouched here (latched crouch, or no room to stand): a crouch jump. The box stays crouch
	// sized for the whole jump, CheckDuck only stands it up when the crouch is let go and it fits

	// consume the assists so a single press can't jump twice
	coyoteTimer = 0;
	jumpBufferTimer = 0;

	// a coyote jump starts while already falling, cancel the downward speed so it is a full jump
	if ( current.velocity * gravityNormal > 0.0f ) {
		current.velocity -= ( current.velocity * gravityNormal ) * gravityNormal;
	}

	groundPlane = false;		// jumping away
	walking = false;
	current.movementFlags |= PMF_JUMP_HELD | PMF_JUMPED;

	addVelocity = 2.0f * maxJumpHeight * -gravityVector;
	addVelocity *= idMath::Sqrt( addVelocity.Normalize() );
	current.velocity += addVelocity;

	return true;
}

/*
=============
idPhysics_Player::UpdateJumpAssists

Refreshes the coyote time while on the ground and buffers jump presses made in the air.
=============
*/
void idPhysics_Player::UpdateJumpAssists( void ) {
	if ( walking ) {
		coyoteTimer = gameLocal.time + PM_COYOTE_MSEC;
		return;
	}

	// pressed in the air after the coyote window: remember it until we land
	if ( command.upmove >= 10 && !( current.movementFlags & PMF_JUMP_HELD ) && coyoteTimer <= gameLocal.time ) {
		jumpBufferTimer = gameLocal.time + PM_JUMPBUFFER_MSEC;
		// latch the press so holding the button doesn't keep refreshing the buffer
		current.movementFlags |= PMF_JUMP_HELD;
	}
}

/*
=============
idPhysics_Player::CheckWaterJump
=============
*/
bool idPhysics_Player::CheckWaterJump( void ) {
	idVec3	spot;
	int		cont;
	idVec3	flatforward;

	if ( current.movementTime ) {
		return false;
	}

	// check for water jump
	if ( waterLevel != WATERLEVEL_WAIST ) {
		return false;
	}

	flatforward = viewForward - (viewForward * gravityNormal) * gravityNormal;
	flatforward.Normalize();

	spot = current.origin + 30.0f * flatforward;
	spot -= 4.0f * gravityNormal;
	cont = gameLocal.clip.Contents( spot, NULL, mat3_identity, -1, self );
	if ( !(cont & CONTENTS_SOLID) ) {
		return false;
	}

	spot -= 16.0f * gravityNormal;
	cont = gameLocal.clip.Contents( spot, NULL, mat3_identity, -1, self );
	if ( cont ) {
		return false;
	}

	// jump out of water
	current.velocity = 200.0f * viewForward - 350.0f * gravityNormal;
	current.movementFlags |= PMF_TIME_WATERJUMP;
	current.movementTime = 2000;

	return true;
}

/*
=============
idPhysics_Player::SetWaterLevel
=============
*/
void idPhysics_Player::SetWaterLevel( void ) {
	idVec3		point;
	idBounds	bounds;
	int			contents;

	//
	// get waterlevel, accounting for ducking
	//
	waterLevel = WATERLEVEL_NONE;
	waterType = 0;

	bounds = clipModel->GetBounds();

	// check at feet level
	point = current.origin - ( bounds[0][2] + 1.0f ) * gravityNormal;
	contents = gameLocal.clip.Contents( point, NULL, mat3_identity, -1, self );
	if ( contents & MASK_WATER ) {

		waterType = contents;
		waterLevel = WATERLEVEL_FEET;

		// check at waist level
		point = current.origin - ( bounds[1][2] - bounds[0][2] ) * 0.5f * gravityNormal;
		contents = gameLocal.clip.Contents( point, NULL, mat3_identity, -1, self );
		if ( contents & MASK_WATER ) {

			waterLevel = WATERLEVEL_WAIST;

			// check at head level
			point = current.origin - ( bounds[1][2] - 1.0f ) * gravityNormal;
			contents = gameLocal.clip.Contents( point, NULL, mat3_identity, -1, self );
			if ( contents & MASK_WATER ) {
				waterLevel = WATERLEVEL_HEAD;
			}
		}
	}
}

/*
================
idPhysics_Player::DropTimers
================
*/
void idPhysics_Player::DropTimers( void ) {
	// drop misc timing counter
	if ( current.movementTime ) {
		if ( framemsec >= current.movementTime ) {
			current.movementFlags &= ~PMF_ALL_TIMES;
			current.movementTime = 0;
		}
		else {
			current.movementTime -= framemsec;
		}
	}
}

/*
================
idPhysics_Player::MovePlayer
================
*/
void idPhysics_Player::MovePlayer( int msec ) {

	// this counter lets us debug movement problems with a journal
	// by setting a conditional breakpoint for the previous frame
	c_pmove++;

	walking = false;
	groundPlane = false;
	ladder = false;

	// determine the time
	framemsec = msec;
	frametime = framemsec * 0.001f;

	// default speed
	playerSpeed = walkSpeed;

	// remove jumped and stepped up flag
	current.movementFlags &= ~(PMF_JUMPED|PMF_STEPPED_UP|PMF_STEPPED_DOWN);
	current.stepUp = 0.0f;

	if ( command.upmove < 10 ) {
		// not holding jump
		current.movementFlags &= ~PMF_JUMP_HELD;
	}

	// the press that started a vault is used up: nothing jumps or vaults again (holding space doesn't
	// re-trigger anything every frame) until jump is fully released AND the vault is over
	if ( command.upmove < 10 && currentVaultState == VAULT_NONE ) {
		vaultLatch = false;
	}

	// vaults start from a jump press, remembered for PM_VAULT_INPUT_BUFFER_MSEC so the timing doesn't have
	// to be exact. Keeping jump held in the air renews that window while rising and around the apex, so
	// jump + hold climbs onto a ledge or into a duct that comes in range mid-air; once falling it runs out.
	// Holding jump on the ground never renews it, and after a vault the latch blocks everything until jump
	// is released. A ledge jump only needs a fresh edge: during the vault that means jump was released
	vaultJumpEdge = ( command.upmove >= 10 ) && !vaultJumpDown;
	vaultJumpPressed = vaultJumpEdge && !vaultLatch;
	vaultJumpDown = ( command.upmove >= 10 );
	if ( vaultLatch ) {
		current.movementFlags |= PMF_JUMP_HELD;
	} else if ( vaultJumpPressed || ( vaultJumpDown && !wasWalking && ( current.velocity * -gravityNormal ) > PM_VAULT_HOLD_MIN_UPSPEED ) ) {
		vaultInputBuffer = gameLocal.time + PM_VAULT_INPUT_BUFFER_MSEC;
	}

	// if no movement at all
	if ( current.movementType == PM_FREEZE ) {
		return;
	}

	// move the player velocity into the frame of a pusher
	current.velocity -= current.pushVelocity;

	// view vectors
	viewAngles.ToVectors( &viewForward, NULL, NULL );
	viewForward *= clipModelAxis;
	viewRight = gravityNormal.Cross( viewForward );
	viewRight.Normalize();

	// fly in spectator mode
	if ( current.movementType == PM_SPECTATOR ) {
		SpectatorMove();
		idPhysics_Player::DropTimers();
		return;
	}

	// special no clip mode
	if ( current.movementType == PM_NOCLIP ) {
		idPhysics_Player::NoclipMove();
		idPhysics_Player::DropTimers();
		return;
	}

	// no control when dead
	if ( current.movementType == PM_DEAD ) {
		command.forwardmove = 0;
		command.rightmove = 0;
		command.upmove = 0;
	}

	// vaulting is kinematic and replaces the regular movement until it is done
	if ( currentVaultState != VAULT_NONE ) {
		if ( current.movementType != PM_NORMAL ) {
			idPhysics_Player::EndVault( false );
		} else {
			idPhysics_Player::ProcessVault( msec );
			if ( currentVaultState == VAULT_NONE ) {
				// back to regular physics: find the ground on top of the ledge
				idPhysics_Player::SetWaterLevel();
				idPhysics_Player::CheckGround();
			}
			current.velocity += current.pushVelocity;
			current.pushVelocity.Zero();
			return;
		}
	}

	// set watertype and waterlevel
	idPhysics_Player::SetWaterLevel();

	// check for ground
	idPhysics_Player::CheckGround();

	// check if up against a ladder
	idPhysics_Player::CheckLadder();

	// slides only live on regular ground
	if ( isSliding && ( !walking || ladder || waterLevel > WATERLEVEL_FEET || current.movementType == PM_DEAD ) ) {
		idPhysics_Player::EndSlide();
	}
	idPhysics_Player::CheckSlideStart();

	// handle timers
	idPhysics_Player::DropTimers();

	// coyote time and jump buffering only apply to regular ground / air movement
	if ( current.movementType == PM_DEAD || ladder || waterLevel > 1 || ( current.movementFlags & PMF_TIME_WATERJUMP ) ) {
		coyoteTimer = 0;
		jumpBufferTimer = 0;
	} else {
		idPhysics_Player::UpdateJumpAssists();
	}

	// jump at an obstacle: vault / ledge grab instead of the regular move. This runs before CheckDuck:
	// the jump press would otherwise stand a crouched player up first, now the vault starts crouched
	if ( idPhysics_Player::CheckVaultStart() ) {
		idPhysics_Player::ProcessVault( msec );
		current.velocity += current.pushVelocity;
		current.pushVelocity.Zero();
		return;
	}

	// set clip model size
	idPhysics_Player::CheckDuck();

	// move
	if ( current.movementType == PM_DEAD ) {
		// dead
		idPhysics_Player::DeadMove();
	}
	else if ( ladder ) {
		// going up or down a ladder
		idPhysics_Player::LadderMove();
	}
	else if ( current.movementFlags & PMF_TIME_WATERJUMP ) {
		// jumping out of water
		idPhysics_Player::WaterJumpMove();
	}
	else if ( waterLevel > 1 ) {
		// swimming
		idPhysics_Player::WaterMove();
	}
	else if ( walking ) {
		if ( isSliding ) {
			// sliding on ground
			idPhysics_Player::ProcessSlide( msec );
		} else {
			// walking on ground
			idPhysics_Player::WalkMove();
		}
	}
	else {
		// airborne, may still jump during coyote time
		idPhysics_Player::CheckJump();
		idPhysics_Player::AirMove();
	}

	// set watertype, waterlevel and groundentity
	idPhysics_Player::SetWaterLevel();
	idPhysics_Player::CheckGround();

	// move the player velocity back into the world frame
	current.velocity += current.pushVelocity;
	current.pushVelocity.Zero();
}

/*
================
idPhysics_Player::GetWaterLevel
================
*/
waterLevel_t idPhysics_Player::GetWaterLevel( void ) const {
	return waterLevel;
}

/*
================
idPhysics_Player::GetWaterType
================
*/
int idPhysics_Player::GetWaterType( void ) const {
	return waterType;
}

/*
================
idPhysics_Player::HasJumped
================
*/
bool idPhysics_Player::HasJumped( void ) const {
	return ( ( current.movementFlags & PMF_JUMPED ) != 0 );
}

/*
================
idPhysics_Player::HasSteppedUp
================
*/
bool idPhysics_Player::HasSteppedUp( void ) const {
	return ( ( current.movementFlags & ( PMF_STEPPED_UP | PMF_STEPPED_DOWN ) ) != 0 );
}

/*
================
idPhysics_Player::GetStepUp
================
*/
float idPhysics_Player::GetStepUp( void ) const {
	return current.stepUp;
}

/*
================
idPhysics_Player::IsCrouching
================
*/
bool idPhysics_Player::IsCrouching( void ) const {
	return ( ( current.movementFlags & PMF_DUCKED ) != 0 );
}

/*
================
idPhysics_Player::OnLadder
================
*/
bool idPhysics_Player::OnLadder( void ) const {
	return ladder;
}

/*
================
idPhysics_Player::idPhysics_Player
================
*/
idPhysics_Player::idPhysics_Player( void ) {
	debugLevel = false;
	clipModel = NULL;
	clipMask = 0;
	memset( &current, 0, sizeof( current ) );
	saved = current;
	walkSpeed = 0;
	crouchSpeed = 0;
	maxStepHeight = 0;
	maxJumpHeight = 0;
	memset( &command, 0, sizeof( command ) );
	viewAngles.Zero();
	framemsec = 0;
	frametime = 0;
	playerSpeed = 0;
	viewForward.Zero();
	viewRight.Zero();
	walking = false;
	groundPlane = false;
	memset( &groundTrace, 0, sizeof( groundTrace ) );
	groundMaterial = NULL;
	ladder = false;
	ladderNormal.Zero();
	coyoteTimer = 0;
	jumpBufferTimer = 0;
	isSprinting = false;
	crouchHeld = false;
	isSliding = false;
	slideTimer = 0;
	slideCooldownTimer = 0;
	wasWalking = false;
	slideDir.Zero();
	currentVaultState = VAULT_NONE;
	vaultType = VAULT_NONE;
	vaultInputBuffer = 0;
	vaultJumpDown = false;
	vaultJumpPressed = false;
	vaultJumpEdge = false;
	vaultLatch = false;
	crouchLatched = false;
	duckOriginShift = 0.0f;
	vaultStartTime = 0;
	vaultPhaseStartTime = 0;
	vaultTimer = 0;
	vaultCooldownTimer = 0;
	vaultStartPos.Zero();
	vaultPhaseStartPos.Zero();
	vaultTargetPos.Zero();
	vaultLedgeNormal.Zero();
	vaultForward.Zero();
	vaultEntryVelocity.Zero();
	vaultLedgeHeight = 0.0f;
	vaultMoveSpeed = 0.0f;
	vaultAbsorbSpeed = 0.0f;
	vaultAbsorbMax = 0.0f;
	vaultControlPos.Zero();
	vaultGraceFrames = 0;
	vaultGraceExpire = 0;
	vaultRiseMsec = 0;
	vaultMantleMsec = 0;
	vaultWallContact = false;
	vaultCrouched = false;
	vaultArc = PM_VAULT_LOW_ARC;
	waterLevel = WATERLEVEL_NONE;
	waterType = 0;
}

/*
================
idPhysics_Player_SavePState
================
*/
void idPhysics_Player_SavePState( idSaveGame *savefile, const playerPState_t &state ) {
	savefile->WriteVec3( state.origin );
	savefile->WriteVec3( state.velocity );
	savefile->WriteVec3( state.localOrigin );
	savefile->WriteVec3( state.pushVelocity );
	savefile->WriteFloat( state.stepUp );
	savefile->WriteInt( state.movementType );
	savefile->WriteInt( state.movementFlags );
	savefile->WriteInt( state.movementTime );
}

/*
================
idPhysics_Player_RestorePState
================
*/
void idPhysics_Player_RestorePState( idRestoreGame *savefile, playerPState_t &state ) {
	savefile->ReadVec3( state.origin );
	savefile->ReadVec3( state.velocity );
	savefile->ReadVec3( state.localOrigin );
	savefile->ReadVec3( state.pushVelocity );
	savefile->ReadFloat( state.stepUp );
	savefile->ReadInt( state.movementType );
	savefile->ReadInt( state.movementFlags );
	savefile->ReadInt( state.movementTime );
}

/*
================
idPhysics_Player::Save
================
*/
void idPhysics_Player::Save( idSaveGame *savefile ) const {

	idPhysics_Player_SavePState( savefile, current );
	idPhysics_Player_SavePState( savefile, saved );

	savefile->WriteFloat( walkSpeed );
	savefile->WriteFloat( crouchSpeed );
	savefile->WriteFloat( maxStepHeight );
	savefile->WriteFloat( maxJumpHeight );
	savefile->WriteInt( debugLevel );

	savefile->WriteUsercmd( command );
	savefile->WriteAngles( viewAngles );

	savefile->WriteInt( framemsec );
	savefile->WriteFloat( frametime );
	savefile->WriteFloat( playerSpeed );
	savefile->WriteVec3( viewForward );
	savefile->WriteVec3( viewRight );

	savefile->WriteBool( walking );
	savefile->WriteBool( groundPlane );
	savefile->WriteTrace( groundTrace );
	savefile->WriteMaterial( groundMaterial );

	savefile->WriteBool( ladder );
	savefile->WriteVec3( ladderNormal );

	savefile->WriteInt( (int)waterLevel );
	savefile->WriteInt( waterType );
}

/*
================
idPhysics_Player::Restore
================
*/
void idPhysics_Player::Restore( idRestoreGame *savefile ) {

	idPhysics_Player_RestorePState( savefile, current );
	idPhysics_Player_RestorePState( savefile, saved );

	savefile->ReadFloat( walkSpeed );
	savefile->ReadFloat( crouchSpeed );
	savefile->ReadFloat( maxStepHeight );
	savefile->ReadFloat( maxJumpHeight );
	savefile->ReadInt( debugLevel );

	savefile->ReadUsercmd( command );
	savefile->ReadAngles( viewAngles );

	savefile->ReadInt( framemsec );
	savefile->ReadFloat( frametime );
	savefile->ReadFloat( playerSpeed );
	savefile->ReadVec3( viewForward );
	savefile->ReadVec3( viewRight );

	savefile->ReadBool( walking );
	savefile->ReadBool( groundPlane );
	savefile->ReadTrace( groundTrace );
	savefile->ReadMaterial( groundMaterial );

	savefile->ReadBool( ladder );
	savefile->ReadVec3( ladderNormal );

	savefile->ReadInt( (int &)waterLevel );
	savefile->ReadInt( waterType );

	/* DG: It can apparently happen that the player saves while the clipModel's axis are
	 *     modified by idPush::TryRotatePushEntity() -> idPhysics_Player::Rotate() -> idClipModel::Link()
	 *     Normally idPush seems to reset them to the identity matrix in the next frame,
	 *     but apparently not when coming from a savegame.
	 *     Usually clipModel->axis is the identity matrix, and if it isn't there's clipping bugs
	 *     like CheckGround() reporting that it's steep even though the player is only trying to
	 *     walk up normal stairs.
	 *     Resetting the axis to mat3_identity when restoring a savegame works around that issue
	 *     and makes sure players can go on playing if their savegame was "corrupted" by saving
	 *     while idPush was active. See https://github.com/dhewm/dhewm3/issues/328 for more details */
	if ( clipModel != NULL ) {
		clipModel->SetPosition( clipModel->GetOrigin(), mat3_identity );
	}
}

/*
================
idPhysics_Player::SetPlayerInput
================
*/
void idPhysics_Player::SetPlayerInput( const usercmd_t &cmd, const idAngles &newViewAngles ) {
	command = cmd;
	viewAngles = newViewAngles;		// can't use cmd.angles cause of the delta_angles
}

/*
================
idPhysics_Player::SetSpeed
================
*/
void idPhysics_Player::SetSpeed( const float newWalkSpeed, const float newCrouchSpeed ) {
	walkSpeed = newWalkSpeed;
	crouchSpeed = newCrouchSpeed;
}

/*
================
idPhysics_Player::SetMaxStepHeight
================
*/
void idPhysics_Player::SetMaxStepHeight( const float newMaxStepHeight ) {
	maxStepHeight = newMaxStepHeight;
}

/*
================
idPhysics_Player::GetMaxStepHeight
================
*/
float idPhysics_Player::GetMaxStepHeight( void ) const {
	return maxStepHeight;
}

/*
================
idPhysics_Player::SetMaxJumpHeight
================
*/
void idPhysics_Player::SetMaxJumpHeight( const float newMaxJumpHeight ) {
	maxJumpHeight = newMaxJumpHeight;
}

/*
================
idPhysics_Player::SetMovementType
================
*/
void idPhysics_Player::SetMovementType( const pmtype_t type ) {
	current.movementType = type;
}

/*
================
idPhysics_Player::SetKnockBack
================
*/
void idPhysics_Player::SetKnockBack( const int knockBackTime ) {
	if ( current.movementTime ) {
		return;
	}
	current.movementFlags |= PMF_TIME_KNOCKBACK;
	current.movementTime = knockBackTime;
}

/*
================
idPhysics_Player::SetDebugLevel
================
*/
void idPhysics_Player::SetDebugLevel( bool set ) {
	debugLevel = set;
}

/*
================
idPhysics_Player::Evaluate
================
*/
bool idPhysics_Player::Evaluate( int timeStepMSec, int endTimeMSec ) {
	idVec3 masterOrigin, oldOrigin;
	idMat3 masterAxis;

	waterLevel = WATERLEVEL_NONE;
	waterType = 0;
	oldOrigin = current.origin;

	clipModel->Unlink();

	// if bound to a master
	if ( masterEntity ) {
		self->GetMasterPosition( masterOrigin, masterAxis );
		current.origin = masterOrigin + current.localOrigin * masterAxis;
		clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() );
		current.velocity = ( current.origin - oldOrigin ) / ( timeStepMSec * 0.001f );
		masterDeltaYaw = masterYaw;
		masterYaw = masterAxis[0].ToYaw();
		masterDeltaYaw = masterYaw - masterDeltaYaw;
		return true;
	}

	ActivateContactEntities();

	idPhysics_Player::MovePlayer( timeStepMSec );

	clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() );

	if ( IsOutsideWorld() ) {
		gameLocal.Warning( "clip model outside world bounds for entity '%s' at (%s)", self->name.c_str(), current.origin.ToString(0) );
	}

	return true; //( current.origin != oldOrigin );
}

/*
================
idPhysics_Player::UpdateTime
================
*/
void idPhysics_Player::UpdateTime( int endTimeMSec ) {
}

/*
================
idPhysics_Player::GetTime
================
*/
int idPhysics_Player::GetTime( void ) const {
	return gameLocal.time;
}

/*
================
idPhysics_Player::GetImpactInfo
================
*/
void idPhysics_Player::GetImpactInfo( const int id, const idVec3 &point, impactInfo_t *info ) const {
	info->invMass = invMass;
	info->invInertiaTensor.Zero();
	info->position.Zero();
	info->velocity = current.velocity;
}

/*
================
idPhysics_Player::ApplyImpulse
================
*/
void idPhysics_Player::ApplyImpulse( const int id, const idVec3 &point, const idVec3 &impulse ) {
	if ( current.movementType != PM_NOCLIP ) {
		current.velocity += impulse * invMass;
	}
}

/*
================
idPhysics_Player::IsAtRest
================
*/
bool idPhysics_Player::IsAtRest( void ) const {
	return false;
}

/*
================
idPhysics_Player::GetRestStartTime
================
*/
int idPhysics_Player::GetRestStartTime( void ) const {
	return -1;
}

/*
================
idPhysics_Player::SaveState
================
*/
void idPhysics_Player::SaveState( void ) {
	saved = current;
}

/*
================
idPhysics_Player::RestoreState
================
*/
void idPhysics_Player::RestoreState( void ) {
	current = saved;

	clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() );

	EvaluateContacts();
}

/*
================
idPhysics_Player::SetOrigin
================
*/
void idPhysics_Player::SetOrigin( const idVec3 &newOrigin, int id ) {
	idVec3 masterOrigin;
	idMat3 masterAxis;

	// teleported: drop any vault in progress
	currentVaultState = VAULT_NONE;

	current.localOrigin = newOrigin;
	if ( masterEntity ) {
		self->GetMasterPosition( masterOrigin, masterAxis );
		current.origin = masterOrigin + newOrigin * masterAxis;
	}
	else {
		current.origin = newOrigin;
	}

	clipModel->Link( gameLocal.clip, self, 0, newOrigin, clipModel->GetAxis() );
}

/*
================
idPhysics_Player::GetOrigin
================
*/
const idVec3 & idPhysics_Player::PlayerGetOrigin( void ) const {
	return current.origin;
}

/*
================
idPhysics_Player::SetAxis
================
*/
void idPhysics_Player::SetAxis( const idMat3 &newAxis, int id ) {
	clipModel->Link( gameLocal.clip, self, 0, clipModel->GetOrigin(), newAxis );
}

/*
================
idPhysics_Player::Translate
================
*/
void idPhysics_Player::Translate( const idVec3 &translation, int id ) {

	current.localOrigin += translation;
	current.origin += translation;

	clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() );
}

/*
================
idPhysics_Player::Rotate
================
*/
void idPhysics_Player::Rotate( const idRotation &rotation, int id ) {
	idVec3 masterOrigin;
	idMat3 masterAxis;

	current.origin *= rotation;
	if ( masterEntity ) {
		self->GetMasterPosition( masterOrigin, masterAxis );
		current.localOrigin = ( current.origin - masterOrigin ) * masterAxis.Transpose();
	}
	else {
		current.localOrigin = current.origin;
	}

	clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() * rotation.ToMat3() );
}

/*
================
idPhysics_Player::SetLinearVelocity
================
*/
void idPhysics_Player::SetLinearVelocity( const idVec3 &newLinearVelocity, int id ) {
	current.velocity = newLinearVelocity;
}

/*
================
idPhysics_Player::GetLinearVelocity
================
*/
const idVec3 &idPhysics_Player::GetLinearVelocity( int id ) const {
	return current.velocity;
}

/*
================
idPhysics_Player::SetPushed
================
*/
void idPhysics_Player::SetPushed( int deltaTime ) {
	idVec3 velocity;
	float d;

	// velocity with which the player is pushed
	velocity = ( current.origin - saved.origin ) / ( deltaTime * idMath::M_MS2SEC );

	// remove any downward push velocity
	d = velocity * gravityNormal;
	if ( d > 0.0f ) {
		velocity -= d * gravityNormal;
	}

	current.pushVelocity += velocity;
}

/*
================
idPhysics_Player::GetPushedLinearVelocity
================
*/
const idVec3 &idPhysics_Player::GetPushedLinearVelocity( const int id ) const {
	return current.pushVelocity;
}

/*
================
idPhysics_Player::ClearPushedVelocity
================
*/
void idPhysics_Player::ClearPushedVelocity( void ) {
	current.pushVelocity.Zero();
}

/*
================
idPhysics_Player::SetMaster

  the binding is never orientated
================
*/
void idPhysics_Player::SetMaster( idEntity *master, const bool orientated ) {
	idVec3 masterOrigin;
	idMat3 masterAxis;

	if ( master ) {
		if ( !masterEntity ) {
			// transform from world space to master space
			self->GetMasterPosition( masterOrigin, masterAxis );
			current.localOrigin = ( current.origin - masterOrigin ) * masterAxis.Transpose();
			masterEntity = master;
			masterYaw = masterAxis[0].ToYaw();
		}
		ClearContacts();
	}
	else {
		if ( masterEntity ) {
			masterEntity = NULL;
		}
	}
}

const float	PLAYER_VELOCITY_MAX				= 4000;
const int	PLAYER_VELOCITY_TOTAL_BITS		= 16;
const int	PLAYER_VELOCITY_EXPONENT_BITS	= idMath::BitsForInteger( idMath::BitsForFloat( PLAYER_VELOCITY_MAX ) ) + 1;
const int	PLAYER_VELOCITY_MANTISSA_BITS	= PLAYER_VELOCITY_TOTAL_BITS - 1 - PLAYER_VELOCITY_EXPONENT_BITS;
const int	PLAYER_MOVEMENT_TYPE_BITS		= 3;
const int	PLAYER_MOVEMENT_FLAGS_BITS		= 8;

/*
================
idPhysics_Player::WriteToSnapshot
================
*/
void idPhysics_Player::WriteToSnapshot( idBitMsgDelta &msg ) const {
	msg.WriteFloat( current.origin[0] );
	msg.WriteFloat( current.origin[1] );
	msg.WriteFloat( current.origin[2] );
	msg.WriteFloat( current.velocity[0], PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	msg.WriteFloat( current.velocity[1], PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	msg.WriteFloat( current.velocity[2], PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	msg.WriteDeltaFloat( current.origin[0], current.localOrigin[0] );
	msg.WriteDeltaFloat( current.origin[1], current.localOrigin[1] );
	msg.WriteDeltaFloat( current.origin[2], current.localOrigin[2] );
	msg.WriteDeltaFloat( 0.0f, current.pushVelocity[0], PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	msg.WriteDeltaFloat( 0.0f, current.pushVelocity[1], PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	msg.WriteDeltaFloat( 0.0f, current.pushVelocity[2], PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	msg.WriteDeltaFloat( 0.0f, current.stepUp );
	msg.WriteBits( current.movementType, PLAYER_MOVEMENT_TYPE_BITS );
	msg.WriteBits( current.movementFlags, PLAYER_MOVEMENT_FLAGS_BITS );
	msg.WriteDeltaInt( 0, current.movementTime );
}

/*
================
idPhysics_Player::ReadFromSnapshot
================
*/
void idPhysics_Player::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	current.origin[0] = msg.ReadFloat();
	current.origin[1] = msg.ReadFloat();
	current.origin[2] = msg.ReadFloat();
	current.velocity[0] = msg.ReadFloat( PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	current.velocity[1] = msg.ReadFloat( PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	current.velocity[2] = msg.ReadFloat( PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	current.localOrigin[0] = msg.ReadDeltaFloat( current.origin[0] );
	current.localOrigin[1] = msg.ReadDeltaFloat( current.origin[1] );
	current.localOrigin[2] = msg.ReadDeltaFloat( current.origin[2] );
	current.pushVelocity[0] = msg.ReadDeltaFloat( 0.0f, PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	current.pushVelocity[1] = msg.ReadDeltaFloat( 0.0f, PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	current.pushVelocity[2] = msg.ReadDeltaFloat( 0.0f, PLAYER_VELOCITY_EXPONENT_BITS, PLAYER_VELOCITY_MANTISSA_BITS );
	current.stepUp = msg.ReadDeltaFloat( 0.0f );
	current.movementType = msg.ReadBits( PLAYER_MOVEMENT_TYPE_BITS );
	current.movementFlags = msg.ReadBits( PLAYER_MOVEMENT_FLAGS_BITS );
	current.movementTime = msg.ReadDeltaInt( 0 );

	if ( clipModel ) {
		clipModel->Link( gameLocal.clip, self, 0, current.origin, clipModel->GetAxis() );
	}
}
