//! dmBotState_Patrol — walk through patrol points in order.
//!
//! The active MoveTo intent is the single source of truth for "reached":
//!  - MoveTo finished (reached) -> dwell, then advance to the next point;
//!  - MoveTo failed (stuck/unreachable) -> log and skip to the next point.
//! After the last point the state returns EXIT.
class dmBotState_Patrol : dmBotState
{
	ref array<vector> m_Route;
	int m_Index = 0;
	float m_DwellTimer = 0.0;
	ref dmBotIntent_MoveTo m_Move;

	override bool CanEnter()
	{
		return GetOwner().GetPatrolPoints().Count() > 0;
	}

	//! Patrol can be preempted (e.g. by a Fight state).
	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		//! Capture the route (copy) — it is not updated during this state.
		ref array<vector> points = GetOwner().GetPatrolPoints();
		m_Route = new array<vector>();
		int i;
		for (i = 0; i < points.Count(); i++)
			m_Route.Insert(points[i]);

		m_Index = 0;
		m_DwellTimer = 0.0;
		m_Move = null;
		if (m_Route.Count() > 0)
			StartMoveToCurrent();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Patrol.entry routePoints=" + m_Route.Count());
		#endif
	}

	override int OnUpdate(float pDt)
	{
		if (m_Route.Count() == 0)
			return EXIT;

		if (!m_Move)
		{
			StartMoveToCurrent();
			return CONTINUE;
		}

		if (m_Move.IsFailed())
		{
			dmBotLog.Error("[FSM] Patrol: точка " + m_Route[m_Index] + " недостижима, пропускаю");
			return Advance();
		}

		if (m_Move.IsFinished())
		{
			m_DwellTimer += pDt;
			if (m_DwellTimer >= DM_PATROL_DWELL_TIME)
				return Advance();
		}
		else
		{
			m_DwellTimer = 0.0;
		}

		return CONTINUE;
	}

	//! Move to the next route point; return EXIT when the route is exhausted.
	int Advance()
	{
		m_Index++;
		if (m_Index >= m_Route.Count())
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Patrol finished (all points done)");
			#endif
			return EXIT;
		}

		m_DwellTimer = 0.0;
		StartMoveToCurrent();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Patrol -> point " + m_Index + "/" + m_Route.Count());
		#endif
		return CONTINUE;
	}

	void StartMoveToCurrent()
	{
		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = m_Route[m_Index];
		m_Move.m_ReachDistance = DM_PATROL_REACH_DISTANCE;
		GetOwner().AddFSMIntent(m_Move);
	}
}
