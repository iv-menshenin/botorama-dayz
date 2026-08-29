//! dmBotIntent_LookAround — stand and periodically look in different directions.
//!
//! IMPORTANT: it re-asserts the current look direction EVERY tick. The arbitration
//! resets the look channel to forward each tick (LookForward), so a look intent
//! must re-write it every tick (like HoldLook) — otherwise the head snaps back to
//! center between direction changes.
class dmBotIntent_LookAround : dmBotIntent
{
	float m_Interval = 3.0;              // секунд между сменой направления
	float m_YawRange = 80.0;             // ±градусов от направления тела
	dmBotLookTurn m_Turn = dmBotLookTurn.AUTO;
	float m_Timer = 0.0;
	float m_CurYaw = 0.0;                // текущее направление взгляда (градусы)

	override void OnStart(dmAISurvivor bot)
	{
		m_Timer = 0.0;
		PickDirection(bot);
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		m_Timer += pDt;
		if (m_Timer >= m_Interval)
		{
			m_Timer = 0.0;
			PickDirection(bot);
		}

		bot.LookAtDirection(m_CurYaw, 0.0, m_Turn);
	}

	void PickDirection(dmAISurvivor bot)
	{
		m_CurYaw = Math.RandomFloat(-m_YawRange, m_YawRange);
		bot.LookAtDirection(m_CurYaw, 0.0, m_Turn);
	}
}
