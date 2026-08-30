//! dmBotIntent_FollowTo — an "improved MoveTo" for escorting a target.
//!
//! Unlike dmBotIntent_MoveTo it is continuous: it never finishes on its own (the
//! owning Follow state removes it via Finish). Every tick it re-derives a dynamic
//! escort anchor — the target's shoulder (±DM_FOLLOW_SIDE_DISTANCE) for a
//! stationary player/bot, a point DM_FOLLOW_SIDE_DISTANCE short of an item, or a
//! side-offset along the motion direction for a moving player/bot (straight onto
//! the target when far / for items). Speed comes from a 1 s extrapolation of both
//! velocities (sprint > DM_FOLLOW_SPRINT_GAP, jog > DM_FOLLOW_JOG_GAP, walk
//! otherwise). The navmesh path is recomputed at most once per
//! DM_FOLLOW_PATH_INTERVAL, aimed at the target's extrapolated position, and
//! re-aimed sooner if the anchor drifts >1 m from where it was last aimed. It
//! steers with SetMoveYaw (body faces the movement) + SetMove and leaves the look
//! channel to a LookAround intent (no LookAtPoint here).
class dmBotIntent_FollowTo : dmBotIntent
{
	EntityAI m_Target;
	float m_SideSign = 1.0;

	vector m_TargetVel;         // сглаженная скорость цели (горизонталь)
	vector m_LastTargetPos;
	bool m_TargetPosKnown = false;

	ref array<vector> m_Path;
	int m_PathIdx;
	float m_PathTimer;
	bool m_HasPath;
	vector m_LastAimPos;        // для переприцеливания пути

	//! Accumulator for the periodic movement debug log (DM_BOT_DEBUG_FSM).
	float m_DebugAccum = 0.0;

	void dmBotIntent_FollowTo()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
	}

	override void OnStart(dmAISurvivor bot)
	{
		m_Path = null;
		m_PathIdx = 0;
		m_PathTimer = DM_FOLLOW_PATH_INTERVAL;
		m_HasPath = false;
		m_TargetPosKnown = false;
		m_TargetVel = vector.Zero;
		m_LastAimPos = vector.Zero;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] FollowTo.start target=" + m_Target);
		#endif
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (!m_Target)
			return;

		//! Positions (locals first — vector arithmetic must not call methods inline).
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
		m_LastTargetPos = targetPos;

		//! Anchor: shoulder for a stationary player/bot, a point short of an item,
		//! a side-offset along the motion for a moving player/bot (or straight onto
		//! the target when far / for items).
		vector toTarget = targetPos - botPos;
		toTarget[1] = 0.0;
		float dist = toTarget.Length();
		float targetSpeed = m_TargetVel.Length();

		vector anchor = vector.Zero;
		vector fwd = vector.Zero;
		vector toItem = vector.Zero;
		vector moveDir = vector.Zero;
		vector side = vector.Zero;
		if (targetSpeed < 0.5)
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
			if (dist > DM_FOLLOW_THRESHOLD_PLAYER || !pb)
			{
				anchor = targetPos;
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
				side = side * m_SideSign;
				anchor = targetPos + side * DM_FOLLOW_SIDE_DISTANCE + m_TargetVel;
			}
		}
		anchor[1] = targetPos[1];

		//! Speed: 1 s extrapolation of both velocities -> gap -> speed index.
		PlayerBase selfPawn = bot.GetPawn();
		vector bv = vector.Zero;
		if (selfPawn)
		{
			selfPawn.PhysicsGetVelocity(bv);
			bv[1] = 0.0;
		}

		vector botFuture = botPos + bv * DM_FOLLOW_SPEED_EXTRAPOLATE_TIME;
		vector targetFuture = targetPos + m_TargetVel * DM_FOLLOW_SPEED_EXTRAPOLATE_TIME;
		vector gap = targetFuture - botFuture;
		gap[1] = 0.0;
		float gapLen = gap.Length();

		float speedIdx = 1.0;
		if (gapLen > DM_FOLLOW_SPRINT_GAP)
			speedIdx = 3.0;
		else if (gapLen > DM_FOLLOW_JOG_GAP)
			speedIdx = 2.0;

		//! Path re-computation: at most 1 Hz, or forced when the anchor drifts >1 m
		//! from where the path was last aimed.
		m_PathTimer += pDt;

		vector aimDrift = anchor - m_LastAimPos;
		aimDrift[1] = 0.0;
		float aimDriftLen = aimDrift.Length();

		if (m_PathTimer >= DM_FOLLOW_PATH_INTERVAL || aimDriftLen > 1.0)
		{
			m_PathTimer = 0.0;
			RePath(bot, targetPos, anchor);
		}

		//! Steering (body faces the movement; the head channel is left to LookAround).
		vector dir = vector.Zero;
		float subDist = 0.0;
		float reach = DM_PATH_WAYPOINT_REACH;
		if (m_HasPath && m_Path.Count() > 0)
		{
			vector subGoal = m_Path[m_PathIdx];
			dir = subGoal - botPos;
			dir[1] = 0.0;
			subDist = dir.Length();

			if (m_PathIdx < m_Path.Count() - 1)
				reach = DM_PATH_WAYPOINT_REACH;
			else
				reach = DM_FOLLOW_REACH;

			if (subDist <= reach)
			{
				if (m_PathIdx < m_Path.Count() - 1)
					m_PathIdx++;
				else
					bot.SetMove(0.0, 0.0);
				return;
			}

			if (subDist < 0.01)
			{
				bot.SetMove(0.0, 0.0);
				return;
			}
		}
		else
		{
			dir = anchor - botPos;
			dir[1] = 0.0;
			subDist = dir.Length();

			if (subDist < DM_FOLLOW_REACH)
			{
				bot.SetMove(0.0, 0.0);
				return;
			}
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);
		bot.SetMoveYaw(subYaw);
		bot.SetMove(moveAngle, speedIdx);

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			dmBotLog.Debug("[FSM] FollowTo: anchor=" + anchor + " gapLen=" + gapLen + " speedIdx=" + speedIdx);
			dmBotLog.Debug("[FSM] FollowTo: hasPath=" + m_HasPath + " targetSpeed=" + targetSpeed + " moveAngle=" + moveAngle + " subDist=" + subDist);
		}
		#endif
	}

	//! Re-aim the navmesh path at the target's extrapolated position (fallback:
	//! the anchor). Keeps the old path when both attempts fail. m_LastAimPos is
	//! updated on every attempt so a persistent failure doesn't re-path each tick.
	void RePath(dmAISurvivor bot, vector targetPos, vector anchor)
	{
		m_LastAimPos = anchor;

		vector pathTarget = targetPos + m_TargetVel * DM_FOLLOW_PATH_EXTRAPOLATE_TIME;
		pathTarget[1] = targetPos[1];

		ref array<vector> newPath = new array<vector>();
		bool ok = bot.FindPathTo(pathTarget, newPath);
		if (!ok || newPath.Count() == 0)
		{
			newPath = new array<vector>();
			ok = bot.FindPathTo(anchor, newPath);
		}

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] FollowTo.RePath: ok=" + ok + " points=" + newPath.Count() + " pathTarget=" + pathTarget);
		#endif

		if (ok && newPath.Count() > 0)
		{
			m_Path = newPath;
			m_PathIdx = 0;
			m_HasPath = true;
			return;
		}

		if (!m_HasPath)
			m_Path = null;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		bot.SetMove(0.0, 0.0);
	}
}
