//! dmBotIntent_HoldLook — stand and keep looking at a point (or an entity).
class dmBotIntent_HoldLook : dmBotIntent
{
	vector m_Point;              // фиксированная точка (если m_Entity не задан)
	EntityAI m_Entity;           // опционально: следим за сущностью каждый тик
	ref dmTarget m_Target;       // опционально: следим за сущностью по ее следам, а не по ее модели в мире
	dmBotLookTurn m_Turn = dmBotLookTurn.AUTO;

	void dmBotIntent_HoldLook()
	{
		m_Manage = dmBotIntentsChannel.LOOK;
	}

	override string GetIntentName()
	{
		return "HoldLook";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		if (m_Target)
		{
			bot.LookAtPoint(m_Target.GetAimPosition(), m_Turn);
			return;
		}

		if (m_Entity)
		{
			vector pos = m_Entity.GetPosition();
			bot.LookAtPoint(pos + Vector(0, DM_EYE_HEIGHT, 0), m_Turn);
		}
		else
			bot.LookAtPoint(m_Point, m_Turn);
	}
}
