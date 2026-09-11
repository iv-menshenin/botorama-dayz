//! dmBotIntent_EscapeDanger — walk away from a danger position (a burning fireplace)
//! by DM_DANGER_ESCAPE_DIST. Personal intent (survives FSM transitions); CRITICAL +
//! EXCLUSIVE so it overrides the FSM movement until the bot is out of danger.
class dmBotIntent_EscapeDanger : dmBotIntent_MoveTo
{
	vector m_DangerPos;

	void dmBotIntent_EscapeDanger()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "EscapeDanger";
	}

	override void OnStart(dmAISurvivor bot)
	{
		vector botPos = bot.GetPosition();
		vector away = botPos - m_DangerPos;
		away[1] = 0.0;
		if (away.Length() < 0.01)
			away = bot.GetDirection();
		away[1] = 0.0;
		away.Normalize();
		m_Goal = botPos + away * DM_DANGER_ESCAPE_DIST;
		m_ReachDistance = 0.5;
		super.OnStart(bot);
	}
}
