//! dmBotState_Patrol — walk through patrol points in order.
class dmBotState_Patrol : dmBotState
{
	ref array<vector> m_Route;
	int m_Index = 0;
	float m_DwellTimer = 0.0;

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
		if (m_Route.Count() > 0)
			StartMoveToCurrent();

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("[FSM] Patrol.entry routePoints=" + m_Route.Count());
		#endif
	}

	override int OnUpdate(float pDt)
	{
		if (m_Route.Count() == 0)
			return EXIT;

		float dist = vector.Distance(GetOwner().GetPosition(), m_Route[m_Index]);
		if (dist <= DM_PATROL_REACH_DISTANCE)
		{
			m_DwellTimer += pDt;
			if (m_DwellTimer >= DM_PATROL_DWELL_TIME)
			{
				m_Index++;
				if (m_Index >= m_Route.Count())
				{
					#ifdef DM_BOT_DEBUG
					dmBotLog.Debug("[FSM] Patrol finished (last point reached)");
					#endif
					return EXIT;
				}
				m_DwellTimer = 0.0;
				StartMoveToCurrent();

				#ifdef DM_BOT_DEBUG
				dmBotLog.Debug("[FSM] Patrol -> point " + m_Index + "/" + m_Route.Count());
				#endif
			}
		}
		else
		{
			m_DwellTimer = 0.0;
		}
		return CONTINUE;
	}

	void StartMoveToCurrent()
	{
		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Target = m_Route[m_Index];
		move.m_ReachDistance = DM_PATROL_REACH_DISTANCE;
		GetOwner().AddFSMIntent(move);
	}
}
