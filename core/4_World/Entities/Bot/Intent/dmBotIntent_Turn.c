//! dmBotIntent_Turn — unconditionally turn the body (and align the head) by a
//! relative angle. Finishes when the body has rotated within the reach tolerance.
class dmBotIntent_Turn : dmBotIntent
{
	float m_Angle = 0.0;       // относительный угол поворота (градусы, ±)
	float m_ReachAngle = 2.0;  // допуск доворота (градусы)
	float m_TargetYaw = 0.0;   // абсолютный целевой yaw (вычисляется в OnStart)

	override void OnStart(dmAISurvivor bot)
	{
		m_TargetYaw = bot.GetOrientation()[0] + m_Angle;
		bot.LookAtYaw(m_TargetYaw, dmBotLookTurn.FULL);
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		bot.LookAtYaw(m_TargetYaw, dmBotLookTurn.FULL);
		float dYaw = bot.GetYawTo(m_TargetYaw);
		if (Math.AbsFloat(dYaw) <= m_ReachAngle)
			Finish();
	}
}
