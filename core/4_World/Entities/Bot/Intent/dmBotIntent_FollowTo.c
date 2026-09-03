//! dmBotIntent_FollowTo — continuous "MoveTo" for escorting a target.
//!
//! It inherits the full path-following machinery from dmBotIntent_MoveTo
//! (steering, stuck detection, vault/climb, ladders, recovery) and only overrides
//! what differs from a plain MoveTo: it is continuous (never finishes on its own —
//! the owning Follow state removes it via Finish), it leaves the head channel to a
//! LookAround intent (KeepLookAtGoal=false), and each tick it re-derives a dynamic
//! escort anchor (UpdateGoal) — the target's shoulder (±DM_FOLLOW_SIDE_DISTANCE)
//! for a stationary player/bot, a point DM_FOLLOW_SIDE_DISTANCE short of an item,
//! or a point DM_FOLLOW_VEL_MULTIPLIER ahead plus a randomized side offset for a
//! moving player/bot. Speed (GetMoveSpeed) comes from the distance to the anchor
//! (sprint > DM_FOLLOW_SPRINT_GAP, jog > DM_FOLLOW_JOG_GAP, walk otherwise), never
//! slower than the target. The navmesh path is recomputed at most once per
//! DM_FOLLOW_PATH_INTERVAL, aimed at the anchor (with no path the bot steers
//! directly toward the anchor).
class dmBotIntent_FollowTo : dmBotIntent_MoveTo
{
	EntityAI m_Target;
	float m_SideSign = 1.0;
	float m_SideDistance = 2.0;

	vector m_TargetVel;         // сглаженная скорость цели (горизонталь)
	bool m_TargetPosKnown = false;

	float m_PathTimer;
	float m_DistToAnchor;
	float m_TargetSpeed;

	vector m_LastPathGoal = vector.Zero;
	bool m_PathGoalValid = false;

	void dmBotIntent_FollowTo()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
		m_ReachDistance = DM_FOLLOW_REACH;
	}

	override string GetIntentName()
	{
		return "FollowTo";
	}

	override bool IsContinuous()
	{
		return true;
	}

	override bool KeepLookAtGoal()
	{
		return false;
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_Path = null;
		m_PathIdx = 0;
		m_PathTimer = DM_FOLLOW_PATH_INTERVAL;
		m_LastPathGoal = vector.Zero;
		m_PathGoalValid = false;
		m_HasPath = false;
		m_TargetPosKnown = false;
		m_TargetVel = vector.Zero;
		m_DoorCheckAccum = 0.0;
	}

	//! Re-derive the dynamic escort anchor into m_Goal and re-path at most once per
	//! DM_FOLLOW_PATH_INTERVAL. Positions first (locals) — vector arithmetic must
	//! not call methods inline.
	override void UpdateGoal(dmAISurvivor bot, float pDt)
	{
		if (!m_Target) return;

		vector targetPos = m_Target.GetPosition();
		vector botPos = bot.GetPosition();

		//! Target velocity (players/bots only), smoothed horizontally. The first
		//! frame is a hard assign so a stale zero doesn't spike the smoothing.
		PlayerBase pb = PlayerBase.Cast(m_Target);
		vector tv = vector.Zero;
		if (pb)
		{
			pb.PhysicsGetVelocity(tv);
			tv[1] = 0.0;
		}

		if (!m_TargetPosKnown)
		{
			m_TargetVel = tv;
			m_TargetPosKnown = true;
		}
		else
		{
			m_TargetVel[0] = m_TargetVel[0] * (1.0 - DM_FOLLOW_VEL_SMOOTH) + tv[0] * DM_FOLLOW_VEL_SMOOTH;
			m_TargetVel[2] = m_TargetVel[2] * (1.0 - DM_FOLLOW_VEL_SMOOTH) + tv[2] * DM_FOLLOW_VEL_SMOOTH;
		}

		m_TargetSpeed = m_TargetVel.Length();

		//! Anchor: shoulder for a stationary player/bot, a point short of an item,
		//! ahead of the target plus a randomized side offset for a moving player/bot.
		vector anchor = vector.Zero;
		vector fwd = vector.Zero;
		vector toItem = vector.Zero;
		vector moveDir = vector.Zero;
		vector side = vector.Zero;
		if (m_TargetSpeed < 0.1)
		{
			if (pb)
			{
				fwd = m_Target.GetDirection();
				fwd[1] = 0.0;
				fwd.Normalize();
				side = Vector(-fwd[2], 0.0, fwd[0]);
				anchor = targetPos + side * (DM_FOLLOW_SIDE_DISTANCE * m_SideSign);
			}
			else
			{
				toItem = botPos - targetPos;
				toItem[1] = 0.0;
				toItem.Normalize();
				anchor = targetPos + toItem * DM_FOLLOW_SIDE_DISTANCE;
			}
		}
		else
		{
			moveDir = m_TargetVel;
			moveDir[1] = 0.0;
			if (moveDir.Length() < 0.01)
				moveDir = m_Target.GetDirection();
			moveDir[1] = 0.0;
			moveDir.Normalize();
			side = Vector(-moveDir[2], 0.0, moveDir[0]);
			side = side * (m_SideDistance * m_SideSign);
			anchor = targetPos + m_TargetVel * DM_FOLLOW_VEL_MULTIPLIER + side;
		}
		anchor[1] = targetPos[1];

		m_Goal = anchor;

		//! Distance to the anchor (horizontal) — feeds the speed selection.
		vector toAnchor = anchor - botPos;
		toAnchor[1] = 0.0;
		m_DistToAnchor = toAnchor.Length();

		//! Path re-computation: at most once per DM_FOLLOW_PATH_INTERVAL, and only
		//! when the anchor actually drifted (a stationary target must NOT re-path
		//! every second — that resets MoveTo's progress monitor and kills stuck
		//! detection / vault).
		m_PathTimer += pDt;
		if (m_PathTimer >= DM_FOLLOW_PATH_INTERVAL)
		{
			if ( m_NoProgressTime >= DM_MOVE_STUCK_TIME / 2 && m_NoProgressTime < DM_MOVE_STUCK_TIME * 2 ) return;
			m_PathTimer = 0.0;
			vector drift = m_Goal - m_LastPathGoal;
			drift[1] = 0.0;
			if (!m_PathGoalValid || drift.Length() > DM_FOLLOW_REPATH_DIST)
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] FollowTo: RePath t=" + m_NoProgressTime);
				if ( bot.m_DebugPlayer )
					GetGame().ChatMP(bot.m_DebugPlayer, "Путь следования изменен t=" + m_NoProgressTime, "colorAction");
				#endif
				m_LastPathGoal = m_Goal;
				m_PathGoalValid = true;
				RePath(bot);
			}
		}
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		// super.OnReachedGoal(bot, pos);
		bot.SetMove(0.0, 0.0);
	}

	//! Speed (0..3) from the distance to the anchor, never slower than the target.
	override float GetMoveSpeed(dmAISurvivor bot)
	{
		float speedIdx = 1.0;

		if (m_DistToAnchor > DM_FOLLOW_SPRINT_GAP)
			speedIdx = 3.0;
		else if (m_DistToAnchor > DM_FOLLOW_JOG_GAP)
			speedIdx = 2.0;
		else if ( m_DistToAnchor < DM_PATH_WAYPOINT_REACH )
			return 0.0;
		else if ( m_DistToAnchor < 1.0 )
			return 1.0;

		float targetSpeedIdx = 1.0;
		if (m_TargetSpeed > DM_SPEED_JOG * 1.2)
			targetSpeedIdx = 3.0;
		else if (m_TargetSpeed > DM_SPEED_WALK * 1.2)
			targetSpeedIdx = 2.0;
		if (targetSpeedIdx > speedIdx)
			speedIdx = targetSpeedIdx;

		return speedIdx;
	}
}
