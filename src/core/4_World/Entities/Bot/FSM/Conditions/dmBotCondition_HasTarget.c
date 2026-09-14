class dmBotCondition_HasTarget : dmBotCondition
{
	bool m_ForHunt;
	bool m_ForFight;

	void dmBotCondition_HasTarget(bool forHunt = true, bool forFight = true)
	{
		m_ForHunt = forHunt;
		m_ForFight = forFight;
	}

	override bool Evaluate(dmAISurvivor bot)
	{
		if ( m_ForHunt && bot.GetHuntTarget() ) return true;
		if ( m_ForFight && bot.GetHostileTarget() ) return true;
		return false;
	}
}
