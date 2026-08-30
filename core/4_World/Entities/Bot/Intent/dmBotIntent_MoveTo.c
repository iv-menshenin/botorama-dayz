//! dmBotIntent_MoveTo — walk to a world point along a navmesh path; finishes when
//! reached.
//!
//! On start it requests a path (bot.FindPathTo) and then follows the waypoints one
//! by one: each tick it steers toward the current waypoint using the body-relative
//! movement command (forward/back/strafe), without snapping the body. It requests
//! the body to face the waypoint (FULL); if a higher-priority look/turn intent owns
//! the body, the movement direction becomes a strafe/backpedal. A per-waypoint
//! progress monitor detects a stuck bot; instead of failing right away it steps
//! back/sideways and re-routes, up to DM_MOVE_MAX_RECOVER times, then gives up.
class dmBotIntent_MoveTo : dmBotIntent
{
	vector m_Target;
	float m_ReachDistance = 0.5;
	float m_ReachDeadline = 0.0;   // seconds to reach the target; 0 = no deadline

	ref array<vector> m_Path;
	int m_PathIdx = 0;

	float m_BestDist = -1.0;
	float m_NoProgressTime = 0.0;

	//! Stuck-recovery: step back/sideways for a short time before re-routing,
	//! up to DM_MOVE_MAX_RECOVER attempts (see OnUpdate).
	bool m_Recovering = false;
	float m_RecoverTimer = 0.0;
	int m_RecoverCount = 0;
	float m_RecoverDir = 180.0;

	//! Accumulator for the periodic movement debug log (DM_BOT_DEBUG_FSM).
	float m_DebugAccum = 0.0;

	//! Accumulator for the proactive door check (throttled by DM_DOOR_CHECK_INTERVAL).
	float m_DoorCheckAccum = 0.0;

	override void OnStart(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.MoveTo.Start");
		#endif

		m_BestDist = -1.0;
		m_NoProgressTime = 0.0;
		m_PathIdx = 0;
		m_DoorCheckAccum = 0.0;

		m_Recovering = false;
		m_RecoverTimer = 0.0;
		m_RecoverCount = 0;

		m_Path = new array<vector>();
		bool hasPath = bot.FindPathTo(m_Target, m_Path);

		#ifdef DM_BOT_DEBUG_FSM
		if (!hasPath)
			dmBotLog.Debug("[FSM] MoveTo.OnStart: FindPathTo=false target=" + m_Target);
		else
			dmBotLog.Debug("[FSM] MoveTo.OnStart: target=" + m_Target + " pathPoints=" + m_Path.Count());
		#endif

		if (!hasPath || m_Path.Count() == 0)
		{
			dmBotLog.Error("MoveTo: нет пути к " + m_Target + " (вне navmesh или недостижимо), abort");
			Fail();
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.MoveTo");
		#endif

		if (IsFinished())
			return;

		if (m_Recovering)
		{
			m_RecoverTimer -= pDt;
			bot.SetMove(m_RecoverDir, 1.0);

			if (m_RecoverTimer > 0.0)
				return;

			m_Recovering = false;
			if (!Recalc(bot))
			{
				dmBotLog.Error("MoveTo: восстановление не помогло, путь к " + m_Target + " недоступен, abort");
				bot.SetMove(0.0, 0.0);
				Fail();
				return;
			}

			m_NoProgressTime = 0.0;
			return;
		}

		m_DoorCheckAccum += pDt;
		if (m_DoorCheckAccum >= DM_DOOR_CHECK_INTERVAL)
		{
			m_DoorCheckAccum = 0.0;
			bot.TryOpenDoorOnPath();
		}

		vector subGoal = m_Path[m_PathIdx];
		float reach = m_ReachDistance;
		if (m_PathIdx < m_Path.Count() - 1)
			reach = DM_PATH_WAYPOINT_REACH;

		vector pos = bot.GetPosition();
		vector dir = subGoal - pos;
		dir[1] = 0.0;
		float dist = dir.Length();

		if (dist <= reach)
		{
			if (m_PathIdx >= m_Path.Count() - 1)
			{
				bot.SetMove(0.0, 0.0);
				Finish();
				return;
			}

			m_PathIdx++;
			m_BestDist = -1.0;
			m_NoProgressTime = 0.0;
			return;
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);

		//! Body faces the movement direction (comfort policy); the head looks at the
		//! waypoint. If a higher-priority look intent holds the body (FULL), moveAngle
		//! becomes the strafe/backpedal direction instead.
		bot.SetMoveYaw(subYaw);
		bot.LookAtPoint(subGoal + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.NONE);
		float speed = bot.CalcSpeed(m_Target, m_ReachDeadline);
		bot.SetMove(moveAngle, speed);

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			dmBotLog.Debug("[FSM] MoveTo: subGoal=" + subGoal + " pos=" + pos + " dist=" + dist + " reach=" + reach + " pathIdx=" + m_PathIdx + " pathPoints=" + m_Path.Count());
			dmBotLog.Debug("[FSM] MoveTo: moveAngle=" + moveAngle + " speed=" + speed + " deadline=" + m_ReachDeadline);
		}
		#endif

		if (m_BestDist < 0.0)
		{
			m_BestDist = dist;
		}
		else if (dist < m_BestDist - DM_MOVE_PROGRESS_EPS)
		{
			m_BestDist = dist;
			m_NoProgressTime = 0.0;
		}
		else
		{
			m_NoProgressTime += pDt;
		}

		if (m_NoProgressTime >= DM_MOVE_STUCK_TIME)
		{
			if (m_Recovering)
				return;

			if (m_RecoverCount < DM_MOVE_MAX_RECOVER)
			{
				m_RecoverCount++;
				m_Recovering = true;
				m_RecoverTimer = DM_MOVE_RECOVER_TIME;
				m_NoProgressTime = 0.0;

				m_RecoverDir = 180.0;
				if (m_RecoverCount % 2 == 0)
				{
					m_RecoverDir = 90.0;
					if (m_RecoverCount % 4 == 0)
						m_RecoverDir = -90.0;
				}

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] MoveTo: stuck, recover #" + m_RecoverCount + " dir=" + m_RecoverDir);
				#endif
				return;
			}

			dmBotLog.Error("MoveTo: застрял на пути к " + m_Target + " (подцель " + subGoal + "), abort");
			bot.SetMove(0.0, 0.0);
			Fail();
		}
	}

	bool Recalc(dmAISurvivor bot)
	{
		ref array<vector> newPath = new array<vector>();
		if (!bot.FindPathTo(m_Target, newPath) || newPath.Count() == 0)
			return false;

		m_Path = newPath;
		m_PathIdx = 0;
		m_BestDist = -1.0;
		m_NoProgressTime = 0.0;
		return true;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		bot.SetMove(0.0, 0.0);
	}
}
