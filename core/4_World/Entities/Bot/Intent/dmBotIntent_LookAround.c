//! dmBotIntent_LookAround — stand and periodically look in different directions.
class dmBotIntent_LookAround : dmBotIntent
{
	float m_Interval = 3.0;              // секунд между сменой направления
	float m_YawRange = 80.0;             // ±градусов от направления тела
	dmBotLookTurn m_Turn = dmBotLookTurn.AUTO;
	float m_Timer = 0.0;

	void OnStart(dmAISurvivor bot)
	{
		m_Timer = 0.0;
		PickDirection(bot);
	}

	void OnUpdate(dmAISurvivor bot, float pDt)
	{
		m_Timer += pDt;
		if (m_Timer >= m_Interval)
		{
			m_Timer = 0.0;
			PickDirection(bot);
		}
	}

	void PickDirection(dmAISurvivor bot)
	{
		float yaw = Math.RandomFloat(-m_YawRange, m_YawRange);
		bot.LookAtDirection(yaw, 0.0, m_Turn);
	}
}
