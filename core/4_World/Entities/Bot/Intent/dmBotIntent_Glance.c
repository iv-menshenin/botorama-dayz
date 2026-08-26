//! dmBotIntent_Glance — look in a relative direction (head only), e.g. a quick
//! "посмотреть" without turning the body. Usually short-lived (deadline).
class dmBotIntent_Glance : dmBotIntent
{
	float m_Angle = 0.0;                        // относительный угол взгляда (градусы)
	dmBotLookTurn m_Turn = dmBotLookTurn.NONE;  // по умолчанию только голова

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		bot.LookAtDirection(m_Angle, 0.0, m_Turn);
	}
}
