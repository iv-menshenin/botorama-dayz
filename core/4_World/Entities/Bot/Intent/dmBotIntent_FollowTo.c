//! dmBotIntent_FollowTo — an "improved MoveTo" for escorting a target.
//!
//! Unlike dmBotIntent_MoveTo it is continuous: it never finishes on its own (the
//! owning Follow state removes it via Finish). Every tick it re-derives a dynamic
//! escort anchor — the target's shoulder (±DM_FOLLOW_SIDE_DISTANCE) for a
//! stationary player/bot, a point DM_FOLLOW_SIDE_DISTANCE short of an item, or a
//! point DM_FOLLOW_VEL_MULTIPLIER ahead plus a randomized side offset for a moving
//! player/bot. Speed comes from the distance to the anchor (sprint >
//! DM_FOLLOW_SPRINT_GAP, jog > DM_FOLLOW_JOG_GAP, walk otherwise), never slower
//! than the target. The navmesh path is recomputed at most once per
//! DM_FOLLOW_PATH_INTERVAL, aimed at the anchor (no fallback chain — with no path
//! the bot steers directly toward the anchor). It steers with SetMoveYaw (body
//! faces the movement) + SetMove and leaves the look channel to a LookAround
//! intent (no LookAtPoint here).
class dmBotIntent_FollowTo : dmBotIntent
{
	EntityAI m_Target;
	float m_SideSign = 1.0;
	float m_SideDistance = 2.0;

	vector m_TargetVel;         // сглаженная скорость цели (горизонталь)
	bool m_TargetPosKnown = false;

	ref array<vector> m_Path;
	int m_PathIdx;
	float m_PathTimer;
	bool m_HasPath;

	//! Accumulator for the periodic movement debug log (DM_BOT_DEBUG_FSM).
	float m_DebugAccum = 0.0;

	//! Accumulator for the proactive door check (throttled by DM_DOOR_CHECK_INTERVAL).
	float m_DoorCheckAccum = 0.0;

	void dmBotIntent_FollowTo()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "FollowTo";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_Path = null;
		m_PathIdx = 0;
		m_PathTimer = DM_FOLLOW_PATH_INTERVAL;
		m_HasPath = false;
		m_TargetPosKnown = false;
		m_TargetVel = vector.Zero;
		m_DoorCheckAccum = 0.0;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] FollowTo.start target=" + m_Target);
		#endif
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.FollowTo");
		#endif

		super.OnUpdate(bot, pDt);

		if (!m_Target)
			return;

		//! Proactive door-open: if a closed door blocks the way, open it (throttled).
		m_DoorCheckAccum += pDt;
		if (m_DoorCheckAccum >= DM_DOOR_CHECK_INTERVAL)
		{
			m_DoorCheckAccum = 0.0;
			bot.TryOpenDoorOnPath();
		}

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

		//! Anchor: shoulder for a stationary player/bot, a point short of an item,
		//! ahead of the target plus a randomized side offset for a moving player/bot.
		float targetSpeed = m_TargetVel.Length();

		vector anchor = vector.Zero;
		vector fwd = vector.Zero;
		vector toItem = vector.Zero;
		vector moveDir = vector.Zero;
		vector side = vector.Zero;
		if (targetSpeed < 0.1)
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

		//! Speed: distance to the anchor, never slower than the target.
		vector toAnchor = anchor - botPos;
		toAnchor[1] = 0.0;
		float distToAnchor = toAnchor.Length();

		float speedIdx = 1.0;
		if (distToAnchor > DM_FOLLOW_SPRINT_GAP)
			speedIdx = 3.0;
		else if (distToAnchor > DM_FOLLOW_JOG_GAP)
			speedIdx = 2.0;

		//! Не отставать: не медленнее скорости цели.
		float targetSpeedIdx = 1.0;
		if (targetSpeed > DM_SPEED_JOG)
			targetSpeedIdx = 3.0;
		else if (targetSpeed > DM_SPEED_WALK)
			targetSpeedIdx = 2.0;
		if (targetSpeedIdx > speedIdx)
			speedIdx = targetSpeedIdx;

		//! Path re-computation: at most once per DM_FOLLOW_PATH_INTERVAL.
		m_PathTimer += pDt;
		if (m_PathTimer >= DM_FOLLOW_PATH_INTERVAL)
		{
			m_PathTimer = 0.0;
			RePath(bot, anchor);
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
			dmBotLog.Debug("[FSM] FollowTo: anchor=" + anchor + " distToAnchor=" + distToAnchor + " speedIdx=" + speedIdx);
			dmBotLog.Debug("[FSM] FollowTo: hasPath=" + m_HasPath + " targetSpeed=" + targetSpeed + " moveAngle=" + moveAngle + " subDist=" + subDist);
		}
		#endif
	}

	//! Re-aim the navmesh path at the escort anchor. No fallback chain: if no path
	//! is found, m_HasPath becomes false and the steering moves directly toward the
	//! anchor.
	void RePath(dmAISurvivor bot, vector anchor)
	{
		ref array<vector> newPath = new array<vector>();
		bool ok = bot.FindPathTo(anchor, newPath);
		if (ok && newPath.Count() > 0)
		{
			m_Path = newPath;
			m_PathIdx = 0;
			m_HasPath = true;
			return;
		}
		m_HasPath = false;
		m_Path = null;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		bot.SetMove(0.0, 0.0);
	}
}
