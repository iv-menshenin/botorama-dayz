//! dmBotState_Idle — stand still; occasionally glance around or turn, then exit
//! so the FSM can transition (e.g. to Patrol).
class dmBotState_Idle : dmBotState
{
	float m_TurnTimer = 0.0;
	float m_TurnInterval = 15.0;
	float m_TotalTimer = 0.0;
	float m_Duration = 30.0;

	override void OnEntry(dmBotState from)
	{
		m_TurnTimer = 0.0;
		m_TurnInterval = Math.RandomFloatInclusive(15.0, 60.0);
		m_TotalTimer = 0.0;
		m_Duration = Math.RandomFloatInclusive(30.0, 90.0);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Idle.entry duration=" + m_Duration);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		m_TurnTimer += pDt;
		if (m_TurnTimer >= m_TurnInterval)
		{
			m_TurnTimer = 0.0;
			m_TurnInterval = Math.RandomFloatInclusive(15.0, 60.0);
			RandomLookOrTurn();
		}

		m_TotalTimer += pDt;
		if (m_TotalTimer >= m_Duration)
			return EXIT;

		return CONTINUE;
	}

	void RandomLookOrTurn()
	{
		dmAISurvivor bot = GetOwner();
		float angle = Math.RandomFloatInclusive(15.0, 120.0);
		int sign = Math.RandomIntInclusive(0, 1);
		if (sign == 1)
			angle = -angle;

		if (Math.AbsFloat(angle) < 45.0 && Math.RandomIntInclusive(0, 1) == 0)
		{
			dmBotIntent_Glance glance = new dmBotIntent_Glance();
			glance.m_Angle = angle;
			glance.m_Deadline = 15.0;
			bot.AddFSMIntent(glance);
		}
		else
		{
			dmBotIntent_Turn turn = new dmBotIntent_Turn();
			turn.m_Angle = angle;
			bot.AddFSMIntent(turn);
		}
	}
}
