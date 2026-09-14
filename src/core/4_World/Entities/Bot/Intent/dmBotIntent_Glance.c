//! dmBotIntent_Glance — look in a relative direction (head only), e.g. a quick
//! "посмотреть" without turning the body. Usually short-lived (deadline).
class dmBotIntent_Glance : dmBotIntent
{
	float m_Angle = 0.0;                        // относительный угол взгляда (градусы)
	dmBotLookTurn m_Turn = dmBotLookTurn.NONE;  // по умолчанию только голова

	void dmBotIntent_Glance()
	{
		m_Manage = dmBotIntentsChannel.LOOK;
	}

	override string GetIntentName()
	{
		return "Glance";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		bot.LookAtDirection(m_Angle, 0.0, m_Turn);
	}
}
