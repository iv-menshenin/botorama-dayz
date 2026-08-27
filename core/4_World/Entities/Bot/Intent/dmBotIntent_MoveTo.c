//! dmBotIntent_MoveTo — walk to a world point along a navmesh path; finishes when
//! reached.
//!
//! On start it requests a path (bot.FindPathTo) and then follows the waypoints one
//! by one: each tick it steers toward the current waypoint using the body-relative
//! movement command (forward/back/strafe), without snapping the body. It requests
//! the body to face the waypoint (FULL); if a higher-priority look/turn intent owns
//! the body, the movement direction becomes a strafe/backpedal. A per-waypoint
//! progress monitor aborts (Fail) and logs when the bot stops getting closer; it
//! re-routes once, then gives up.
class dmBotIntent_MoveTo : dmBotIntent
{
	vector m_Target;
	float m_ReachDistance = 0.5;
	float m_ReachDeadline = 0.0;   // seconds to reach the target; 0 = no deadline

	ref array<vector> m_Path;
	int m_PathIdx = 0;
	int m_RecalcCount = 0;

	float m_BestDist = -1.0;
	float m_NoProgressTime = 0.0;

	override void OnStart(dmAISurvivor bot)
	{
		m_BestDist = -1.0;
		m_NoProgressTime = 0.0;
		m_RecalcCount = 0;
		m_PathIdx = 0;

		m_Path = new array<vector>();
		if (!bot.FindPathTo(m_Target, m_Path) || m_Path.Count() == 0)
		{
			dmBotLog.Error("MoveTo: нет пути к " + m_Target + " (вне navmesh или недостижимо), abort");
			Fail();
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (IsFinished())
			return;

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

		//! Stop-turn-walk: if the body faces away from the movement direction, stop
		//! and let the idle foot-step turn rotate it (the moving slide-turn is broken
		//! — vanilla heading model overrides it). Pause the progress monitor.
		if (Math.AbsFloat(moveAngle) > DM_MOVE_FACE_THRESHOLD)
		{
			bot.LookAtPoint(subGoal + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.FULL);
			bot.SetMove(0.0, 0.0);
			m_NoProgressTime = 0.0;
			return;
		}

		//! Request the body to face the waypoint. If a higher-priority look intent
		//! owns the body, moveAngle becomes the strafe/backpedal direction instead.
		bot.LookAtPoint(subGoal + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.FULL);
		bot.SetMove(moveAngle, bot.CalcSpeed(m_Target, m_ReachDeadline));

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
			//! Stuck: re-route once, then give up.
			if (Recalc(bot))
				return;

			dmBotLog.Error("MoveTo: застрял на пути к " + m_Target + " (подцель " + subGoal + "), abort");
			bot.SetMove(0.0, 0.0);
			Fail();
		}
	}

	bool Recalc(dmAISurvivor bot)
	{
		if (m_RecalcCount >= DM_MOVE_MAX_RECALC)
			return false;
		m_RecalcCount++;

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
