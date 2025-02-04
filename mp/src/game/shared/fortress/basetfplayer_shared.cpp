/*
Copyright (C) Valve Corporation
Copyright (C) 2014-2017 TalonBrave.info
*/

// Purpose: TF2's player object, code shared between client & server.

#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combatshield.h"
#include "weapon_objectselection.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_builder.h"
#ifdef CLIENT_DLL
#include "prediction.h"
#else
#include "basegrenade_shared.h"
#include "grenade_objectsapper.h"
#endif

bool CBaseTFPlayer::IsClass( TFClass iClass )
{
	if ( !GetPlayerClass()  )
	{
		// Special case for undecided players
		if ( iClass == TFCLASS_UNDECIDED )
			return true;
		return false;
	}

	return ( PlayerClass() == iClass );
}

CWeaponCombatShield *CBaseTFPlayer::GetCombatShield( void )
{
	if ( !m_hWeaponCombatShield )
	{
		if ( GetTeamNumber() == TEAM_ALIENS )
		{
			m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( Weapon_OwnsThisType( "weapon_combat_shield_alien" ) );
#ifndef CLIENT_DLL
			if ( !m_hWeaponCombatShield )
				m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( GiveNamedItem( "weapon_combat_shield_alien" ) );
#endif
		}
		else
		{
			m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( Weapon_OwnsThisType( "weapon_combat_shield" ) );
#ifndef CLIENT_DLL
			if ( !m_hWeaponCombatShield )
				m_hWeaponCombatShield = static_cast< CWeaponCombatShield * >( GiveNamedItem( "weapon_combat_shield" ) );
#endif
		}
	}

	return m_hWeaponCombatShield;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the shot is blocked by the player's handheld shield
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsHittingShield( const Vector &vecVelocity, float *flDamage )
{
	if (!IsParrying() && !IsBlocking())
		return false;

	Vector2D vecDelta = vecVelocity.AsVector2D();
	Vector2DNormalize( vecDelta );

	Vector forward;
	AngleVectors( GetLocalAngles(), &forward );

	Vector2DNormalize( forward.AsVector2D() );

	float flDot = DotProduct2D( vecDelta, forward.AsVector2D() );

	// This gives us a little more than a 90 degree protection angle
	if (flDot < -0.67f)
	{
		// We've hit the players handheld shield, see if the shield can do anything about it
		if ( flDamage && GetCombatShield() )
		{
			// Return true if the shield blocked it all
			*flDamage = GetCombatShield()->AttemptToBlock( *flDamage );
			return ( !(*flDamage) );
		}
		
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound to show we've been hurt
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PainSound( void )
{
	static const char *humanSoundList[ TFCLASS_CLASS_COUNT ] = {
		nullptr,
		"HumanRecon.Pain",
		"HumanCommando.Pain",
		"HumanMedic.Pain",
		"HumanDefender.Pain",
		"HumanSniper.Pain",
		"HumanSupport.Pain",
		"HumanEscort.Pain",
		"HumanSapper.Pain",
		"HumanInfiltrator.Pain",
		"HumanPyro.Pain",
	};
	static const char *alienSoundList[ TFCLASS_CLASS_COUNT ] = {
		nullptr,
		"AlienRecon.Pain",
		"AlienCommando.Pain",
		"AlienMedic.Pain",
		"AlienDefender.Pain",
		"AlienSniper.Pain",
		"AlienSupport.Pain",
		"AlienEscort.Pain",
		"AlienSapper.Pain",
		"AlienInfiltrator.Pain",
		"AlienPyro.Pain",
	};

	const char *sSoundName = nullptr;

	int playerClassNum = PlayerClass();
	if ( GetTeamNumber() == TEAM_ALIENS ) {
		sSoundName = alienSoundList[ playerClassNum ];
	} else {
		sSoundName = humanSoundList[ playerClassNum ];
	}

	if ( sSoundName == nullptr )
		return;

	CPASAttenuationFilter filter( this, sSoundName );
	EmitSound( filter, entindex(), sSoundName );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should record our last weapon when switching between the two specified weapons
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon )
{
	// Don't record last weapons when switching to an object
	if ( dynamic_cast< CWeaponObjectSelection* >( pNewWeapon ) )
	{
		// Store this weapon off so we can switch back to it
		// Don't store it if it's also an object
		CBaseCombatWeapon *pLast = pOldWeapon->GetLastWeapon();
#ifdef CLIENT_DLL
		if ( !dynamic_cast< C_WeaponBuilder* >( pLast ) )
#else
		if ( !dynamic_cast< CWeaponBuilder* >( pLast ) )
#endif
			m_hLastWeaponBeforeObject = pLast;

		return false;
	}

	// Don't record last weapons when switching from the builder
	// If the old weapon is a twohanded container, check the left weapon
	CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pOldWeapon );
	if ( pContainer )
		pOldWeapon = dynamic_cast< CBaseTFCombatWeapon  * >( pContainer->GetLeftWeapon() );

#ifdef CLIENT_DLL
	if ( dynamic_cast< C_WeaponBuilder* >( pOldWeapon ) )
		return false;
#else
	if ( dynamic_cast< CWeaponBuilder* >( pOldWeapon ) )
		return false;
#endif

	return BaseClass::Weapon_ShouldSetLast( pOldWeapon, pNewWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should allow selection of the specified item
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon )
{
	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();

	// If the old weapon is a twohanded container, check the left weapon
	CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pActiveWeapon );
	if ( pContainer )
		pActiveWeapon = pContainer->GetLeftWeapon();

	return ( pWeapon != pActiveWeapon );
}

#ifndef CLIENT_DLL
// Sapper handling is all here because it'll soon be shared Client / Server

bool CBaseTFPlayer::IsAttachingSapper( void )
{
	return ( m_TFLocal.m_bAttachingSapper );
}

float CBaseTFPlayer::GetSapperAttachmentTime( void )
{
	return (gpGlobals->curtime - m_flSapperAttachmentStartTime);
}

void CBaseTFPlayer::StartAttachingSapper( CBaseObject *pObject, CGrenadeObjectSapper *pSapper )
{
	Assert( pSapper );

	m_TFLocal.m_bAttachingSapper = true;
	m_TFLocal.m_flSapperAttachmentFrac = 0.0f;

	m_hSappedObject = pObject;
	m_flSapperAttachmentStartTime = gpGlobals->curtime;
	m_flSapperAttachmentFinishTime = gpGlobals->curtime + m_hSappedObject->GetSapperAttachTime();
	m_hSapper = pSapper;
	m_hSapper->SetArmed( false );

	CPASAttenuationFilter filter( m_hSapper, "WeaponObjectSapper.Attach" );
	EmitSound( filter, m_hSapper->entindex(), "WeaponObjectSapper.Attach" );

	// Drop the player's weapon
	if ( GetActiveWeapon() )
		GetActiveWeapon()->Holster();
}

void CBaseTFPlayer::CheckSapperAttaching( void )
{
	// Did we stop attaching?
	if ( !m_TFLocal.m_bAttachingSapper )
	{
		if ( m_TFLocal.m_flSapperAttachmentFrac )
			StopAttaching();

		return;
	}

	// Object gone?
	if ( m_hSappedObject == NULL )
	{
		StopAttaching();
		return;
	}

	// Sapper gone?
	if ( m_hSapper == NULL )
	{
		StopAttaching();
		return;
	}

	// Make sure I'm still looking at the target
	trace_t tr;
	Vector vecAiming;
	Vector vecSrc = EyePosition();
	EyeVectors( &vecAiming );
	UTIL_TraceLine( vecSrc, vecSrc + (vecAiming * 128), MASK_SOLID, this, TFCOLLISION_GROUP_WEAPON, &tr );
	if ( tr.fraction == 1.0 || tr.m_pEnt != m_hSappedObject )
	{
		StopAttaching();
		return;
	}

	// Finished?
	if ( m_flSapperAttachmentFinishTime >= gpGlobals->curtime )
	{
		float dt = m_flSapperAttachmentFinishTime - m_flSapperAttachmentStartTime;
		if ( dt > 0.0f )
		{
			m_TFLocal.m_flSapperAttachmentFrac = ( gpGlobals->curtime - m_flSapperAttachmentStartTime ) / dt;
			m_TFLocal.m_flSapperAttachmentFrac = clamp( m_TFLocal.m_flSapperAttachmentFrac, 0.0f, 1.0f );
		}
		else
			m_TFLocal.m_flSapperAttachmentFrac = 0.0f;

		return;
	}

	FinishAttaching();
}

void CBaseTFPlayer::CleanupAfterAttaching( void )
{
	Assert( m_TFLocal.m_bAttachingSapper );
	m_TFLocal.m_bAttachingSapper		= false;
	m_flSapperAttachmentFinishTime		= -1;
	m_flSapperAttachmentStartTime		= -1;
	m_TFLocal.m_flSapperAttachmentFrac	= 0.0f;

	// Restore the player's weapon
	m_flNextAttack = gpGlobals->curtime;
	if ( GetActiveWeapon() )
		GetActiveWeapon()->Deploy();
}

void CBaseTFPlayer::StopAttaching( void )
{
	CleanupAfterAttaching();	

	if ( m_hSapper != NULL ) {
		m_hSapper->CleanUp();
		m_hSapper = nullptr;
	}
}

void CBaseTFPlayer::FinishAttaching( void )
{
	CleanupAfterAttaching();

	if ( m_hSapper != NULL )
	{
		m_hSapper->SetTargetObject( m_hSappedObject );
		m_hSapper->SetArmed( true );
	}
}

#endif

//-----------------------------------------------------------------------------
// Plants player footprint decals
//-----------------------------------------------------------------------------

// FIXME FIXME:  Does this need to be hooked up?
bool CBaseTFPlayer::IsWet() const
{
#ifdef CLIENT_DLL
	return ((GetFlags() & FL_INRAIN) != 0) || (m_WetTime >= gpGlobals->curtime);
#else
	return ((GetFlags() & FL_INRAIN) != 0);
#endif
}

#define PLAYER_HALFWIDTH 12

#include "decals.h"

/*	Brought over from gamemovement.
*/
void CBaseTFPlayer::PlantFootprint( surfacedata_t *psurface, const char *cMaterialStep )
{
	// Can't plant footprints on fake materials (ladders, wading)
	if ( psurface->game.material != 'X' )
	{
#if 0
		else
		{
			// FIXME: Activate this once we decide to pull the trigger on it.
			// NOTE: We could add in snow, mud, others here
			switch(psurface->game.material)
			{
			case CHAR_TEX_DIRT:
				Q_snprintf( cDecal, sizeof(cDecal), "Footprint.Dirt"  );
				break;
			default:
				Q_snprintf( cDecal, sizeof(cDecal), "Footprint.Default"  );
			}
		}
#endif
		// Only support dirt for now, ugh... ~hogsy
		if(psurface->game.material != CHAR_TEX_DIRT)
			return;

		char	cDecal[256];

		// Figure out which footprint type to plant...
		// Use the wet footprint if we're wet...
		if (IsWet())
			V_snprintf(cDecal,sizeof(cDecal),"Footprint.Wet");
		else
			V_snprintf(cDecal,sizeof(cDecal),"Footprint.%s",cMaterialStep);

		Vector right;
		AngleVectors(GetLocalAngles(), 0, &right, 0 );
		
		// Figure out where the top of the stepping leg is 
		trace_t tr;
		Vector hipOrigin;
		VectorMA( GetLocalOrigin(), 
			// Reversed this, since it was the wrong way round. ~hogsy
			m_Local.m_nStepside ? PLAYER_HALFWIDTH : -PLAYER_HALFWIDTH,
			right, hipOrigin );
		
		// Find where that leg hits the ground
		// Use the step size... Just happens to be the right sort of size... ~hogsy
		UTIL_TraceLine( hipOrigin, hipOrigin + Vector(0, 0, -PLAYERCLASS_STEPSIZE), 
						MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
		// Return if we're not hitting anything ~hogsy
		if(!tr.m_pEnt)
			return;
		
		// Splat a decal
		CPVSFilter filter( tr.endpos );
		
		te->FootprintDecal(filter,0.0f,&tr.endpos,&right,tr.m_pEnt->entindex(),decalsystem->GetDecalIndexForName(cDecal),psurface->game.material);
	}
}

#define WET_TIME	5.f		// how many seconds till we're completely wet/dry
#define DRY_TIME	20.f	// how many seconds till we're completely wet/dry

void CBaseTFPlayer::UpdateWetness(void)
{
	// BRJ 1/7/01
	// Check for whether we're in a rainy area....
	// Do this by tracing a line straight down with a size guaranteed to
	// be larger than the map
	// Update wetness based on whether we're in rain or not...
#ifdef CLIENT_DLL
	bool bInWater = (UTIL_PointContents ( GetAbsOrigin() ) & MASK_WATER) ? true : false;

	// TODO: Also need to support actual rain, and not just water contents!! ~hogsy
	if(bInWater)
	{
		if (! (GetFlags() & FL_INRAIN) )
		{
			// Transition...
			// Figure out how wet we are now (we were drying off...)
			float wetness = (m_WetTime - gpGlobals->curtime) / DRY_TIME;
			if (wetness < 0.0f)
				wetness = 0.0f;

			// Here, wet time represents the time at which we get totally wet
			m_WetTime = gpGlobals->curtime + (1.0 - wetness) * WET_TIME; 

			AddFlag(FL_INRAIN);
		}
	}
	else
	{
		if ((GetFlags() & FL_INRAIN) != 0)
		{
			// Transition...
			// Figure out how wet we are now (we were getting more wet...)
			float wetness = 1.0f + (gpGlobals->curtime - m_WetTime) / WET_TIME;
			if (wetness > 1.0f)
				wetness = 1.0f;

			// Here, wet time represents the time at which we get totally dry
			m_WetTime = gpGlobals->curtime + wetness * DRY_TIME; 

			RemoveFlag(FL_INRAIN);
		}
	}
#endif
}

bool CBaseTFPlayer::ShouldPlayStepSound( surfacedata_t *psurface, Vector &vecOrigin )
{
	if ( !GetLadderSurface(vecOrigin) && Vector2DLength( GetAbsVelocity().AsVector2D() ) <= 100 )
		return false;

	return true;
}

extern ConVar	sv_footsteps;

/*	Pulled over from tf_gamemovement. ~hogsy
*/
void CBaseTFPlayer::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	if ( gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat() )
		return;

	if ( !psurface )
		return;

	if (!force && !ShouldPlayStepSound( psurface, vecOrigin ))
		return;

	unsigned short stepSoundName = m_Local.m_nStepside ? psurface->sounds.stepleft : psurface->sounds.stepright;
	m_Local.m_nStepside = !m_Local.m_nStepside;

	IPhysicsSurfaceProps *physprops = MoveHelper( )->GetSurfaceProps();
	const char *pSoundName = physprops->GetString( stepSoundName );
	char szSound[256];

	if ( !stepSoundName )
		return;

	// TODO:  See note above, should this be hooked up?
	// Moved down here, since we can check our step side above ~hogsy
	PlantFootprint( psurface, pSoundName );

#if defined( CLIENT_DLL )
	// during prediction play footstep sounds only once
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
#endif

	// Prepend our team's footsteps
	if ( GetTeamNumber() == TEAM_HUMANS )
		V_snprintf( szSound, sizeof(szSound), "Human.%s", pSoundName );
	else if ( GetTeamNumber() == TEAM_ALIENS )
		V_snprintf( szSound, sizeof(szSound), "Alien.%s", pSoundName );
	else
		return;

	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( szSound, params, NULL ) )
		return;

	MoveHelper( )->StartSound( vecOrigin, CHAN_BODY, params.soundname, fvol, params.soundlevel, 0, params.pitch );
}	

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	45.0f
// After this, need to start turning feet
#define MAX_TORSO_ANGLE		90.0f
// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION		15.0f

static ConVar tf2_facefronttime( "tf2_facefronttime", "5", FCVAR_REPLICATED, "After this amount of time of standing in place but aiming to one side, go ahead and move feet to face upper body." );
static ConVar tf2_feetyawrate( "tf2_feetyawrate", "720", FCVAR_REPLICATED, "How many degress per second that we can turn our feet or upper body." );
static ConVar tf2_feetyawrunscale( "tf2_feetyawrunscale", "2", FCVAR_REPLICATED, "Multiplier on tf2_feetyawrate to allow turning faster when running." );
static ConVar tf2_ik( "tf2_ik", "1", FCVAR_REPLICATED, "Use IK on in-place turns." );
extern ConVar sv_backspeed;

CPlayerAnimState::CPlayerAnimState( CBaseTFPlayer *outer )
	: m_pOuter( outer )
{
	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_flCurrentTorsoYaw = 0.0f;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::Update()
{
	m_angRender = GetOuter()->GetLocalAngles();

	ComputePoseParam_BodyYaw();
	ComputePoseParam_BodyPitch();
	ComputePoseParam_BodyLookYaw();

	ComputePlaybackRate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePlaybackRate()
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();

	bool isMoving = ( speed > 0.5f ) ? true : false;

	Activity currentActivity = 	GetOuter()->GetSequenceActivity( GetOuter()->GetSequence() );

	switch ( currentActivity )
	{
	case ACT_WALK:
	case ACT_RUN:
	case ACT_IDLE:
		{
			float maxspeed = GetOuter()->MaxSpeed();
			if ( isMoving && ( maxspeed > 0.0f ) )
			{
				float flFactor = 1.0f;

				// HACK HACK:: Defender backward animation is animated at 0.6 times speed, so scale up animation for this class
				//  if he's running backward.

				// Not sure if we're really going to do all classes this way.
				if ( GetOuter()->IsClass( TFCLASS_DEFENDER ) ||
					 GetOuter()->IsClass( TFCLASS_MEDIC ) )
				{
					Vector facing;
					Vector moving;

					moving = vel;
					AngleVectors( GetOuter()->GetLocalAngles(), &facing );
					VectorNormalize( moving );

					float dot = moving.Dot( facing );
					if ( dot < 0.0f )
					{
						float backspeed = sv_backspeed.GetFloat();
						flFactor = 1.0f - fabs( dot ) * (1.0f - backspeed);

						if ( flFactor > 0.0f )
						{
							flFactor = 1.0f / flFactor;
						}
					}
				}

				// Note this gets set back to 1.0 if sequence changes due to ResetSequenceInfo below
				GetOuter()->SetPlaybackRate( ( speed * flFactor ) / maxspeed );

				// BUG BUG:
				// This stuff really should be m_flPlaybackRate = speed / m_flGroundSpeed
			}
			else
			{
				GetOuter()->SetPlaybackRate( 1.0f );
			}
		}
		break;
	default:
		{
			GetOuter()->SetPlaybackRate( 1.0f );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBaseTFPlayer *CPlayerAnimState::GetOuter()
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::EstimateYaw( void )
{
	float dt = gpGlobals->frametime;

	if ( !dt )
	{
		return;
	}

	Vector est_velocity;
	QAngle	angles;

	GetOuterAbsVelocity( est_velocity );

	angles = GetOuter()->GetLocalAngles();

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = angles[YAW] - m_flGaitYaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_flGaitYaw += flYawDiff;
		m_flGaitYaw = m_flGaitYaw - (int)(m_flGaitYaw / 360) * 360;
	}
	else
	{
		m_flGaitYaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);

		if (m_flGaitYaw > 180)
			m_flGaitYaw = 180;
		else if (m_flGaitYaw < -180)
			m_flGaitYaw = -180;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyYaw( void )
{
	int iYaw = GetOuter()->LookupPoseParameter( "move_yaw" );
	if ( iYaw < 0 )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	QAngle	angles = GetOuter()->GetLocalAngles();
	float ang = angles[ YAW ];
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}
	
	GetOuter()->SetPoseParameter( iYaw, flYaw );
}

void CPlayerAnimState::ComputePoseParam_BodyPitch( void )
{
	// Get pitch from v_angle
	float flPitch = GetOuter()->GetLocalAngles()[ PITCH ];
	if ( flPitch > 180.0f )
		flPitch -= 360.0f;

	flPitch = clamp( flPitch, -90, 90 );

	m_angRender.x = 0.0f;

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( "body_pitch" );
	if ( pitch < 0 )
		return;

	GetOuter()->SetPoseParameter( pitch, flPitch );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CPlayerAnimState::ConvergeAngles( float goal,float maxrate, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	float anglediffabs = fabs( anglediff );

	anglediff = AngleNormalize( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( fabs( anglediff ) < maxmove )
	{
		current = goal;
	}
	else
	{
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CPlayerAnimState::ComputePoseParam_BodyLookYaw( void )
{
	m_angRender.y = AngleNormalize( m_angRender.y );

	// See if we even have a blender for pitch
	int upper_body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );
	if ( upper_body_yaw < 0 )
		return;

	// Assume upper and lower bodies are aligned and that we're not turning
	float flGoalTorsoYaw = 0.0f;
	int turning = TURN_NONE;
	float turnrate = tf2_feetyawrate.GetFloat();

	Vector vel;
	
	GetOuterAbsVelocity( vel );

	bool isMoving = ( vel.Length() > 1.0f ) ? true : false;
	if ( !isMoving )
	{
		// Just stopped moving, try and clamp feet
		if ( m_flLastTurnTime <= 0.0f )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAbsAngles().y;
			// Snap feet to be perfectly aligned with torso/eyes
			m_flGoalFeetYaw		= GetOuter()->GetAbsAngles().y;
			m_flCurrentFeetYaw	= m_flGoalFeetYaw;
			m_nTurningInPlace	= TURN_NONE;
		}

		// If rotating in place, update stasis timer
		if ( m_flLastYaw != GetOuter()->GetAbsAngles().y )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAbsAngles().y;
		}

		if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
		}

		turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );

		// See how far off current feetyaw is from true yaw
		float yawdelta = GetOuter()->GetAbsAngles().y - m_flCurrentFeetYaw;
		yawdelta = AngleNormalize( yawdelta );

		bool rotated_too_far = false;

		float yawmagnitude = fabs( yawdelta );
		// If too far, then need to turn in place
		if ( yawmagnitude > MAX_TORSO_ANGLE )
		{
			rotated_too_far = true;
		}

		// Standing still for a while, rotate feet around to face forward
		// Or rotated too far
		// FIXME:  Play an in place turning animation
		if ( rotated_too_far || 
			( gpGlobals->curtime > m_flLastTurnTime + tf2_facefronttime.GetFloat() ) )
		{
			m_flGoalFeetYaw		= GetOuter()->GetAbsAngles().y;
			m_flLastTurnTime	= gpGlobals->curtime;

			float yd = m_flCurrentFeetYaw - m_flGoalFeetYaw;
			if ( yd > 0 )
				m_nTurningInPlace = TURN_RIGHT;
			else if ( yd < 0 )
				m_nTurningInPlace = TURN_LEFT;
			else
				m_nTurningInPlace = TURN_NONE;

			turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
			yawdelta = GetOuter()->GetAbsAngles().y - m_flCurrentFeetYaw;
		}

		// Snap upper body into position since the delta is already smoothed for the feet
		flGoalTorsoYaw = yawdelta;
		m_flCurrentTorsoYaw = flGoalTorsoYaw;
	}
	else
	{
		m_flLastTurnTime = 0.0f;
		m_nTurningInPlace = TURN_NONE;
		m_flGoalFeetYaw = GetOuter()->GetAbsAngles().y;
		flGoalTorsoYaw = 0.0f;
		turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
		m_flCurrentTorsoYaw = GetOuter()->GetAbsAngles().y - m_flCurrentFeetYaw;
	}


	if ( turning == TURN_NONE )
	{
		m_nTurningInPlace = turning;
	}

	if ( m_nTurningInPlace != TURN_NONE )
	{
		// If we're close to finishing the turn, then turn off the turning animation
		if ( fabs( m_flCurrentFeetYaw - m_flGoalFeetYaw ) < MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION )
		{
			m_nTurningInPlace = TURN_NONE;
		}
	}

	// Counter rotate upper body as needed
	ConvergeAngles( flGoalTorsoYaw, turnrate, gpGlobals->frametime, m_flCurrentTorsoYaw );

	// Rotate entire body into position
	m_angRender.y = m_flCurrentFeetYaw;

	GetOuter()->SetPoseParameter( upper_body_yaw, clamp( m_flCurrentTorsoYaw, -90.0f, 90.0f ) );
}

Activity CPlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	/*
	case TURN_RIGHT:
		return ACT_TURNRIGHT45;
	case TURN_LEFT:
		return ACT_TURNLEFT45;
	*/
	case TURN_RIGHT:
	case TURN_LEFT:
		return tf2_ik.GetBool() ? ACT_TURN : activity;
	}

	Assert( 0 );
	return activity;
}

const QAngle& CPlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

void CPlayerAnimState::GetOuterAbsVelocity( Vector& vel )
{
#if defined( CLIENT_DLL )
	GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}