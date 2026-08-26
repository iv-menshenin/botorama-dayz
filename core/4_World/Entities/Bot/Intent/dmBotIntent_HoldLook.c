//! dmBotIntent_HoldLook — stand and keep looking at a point (or an entity).
class dmBotIntent_HoldLook : dmBotIntent
{
	vector m_Point;              // фиксированная точка (если m_Entity не задан)
	EntityAI m_Entity;           // опционально: следим за сущностью каждый тик
	dmBotLookTurn m_Turn = dmBotLookTurn.AUTO;

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (m_Entity)
			bot.LookAtPoint(m_Entity.GetPosition() + Vector(0, DM_EYE_HEIGHT, 0), m_Turn);
		else
			bot.LookAtPoint(m_Point, m_Turn);
	}
}
