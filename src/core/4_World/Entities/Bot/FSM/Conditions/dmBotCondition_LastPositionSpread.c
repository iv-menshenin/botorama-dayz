class dmBotCondition_LastPositionSpread : dmBotCondition
{
    float m_Radius;
    bool m_LessThan;

    void dmBotCondition_LastPositionSpread(float r, bool less)
    {
        m_Radius = r;
        m_LessThan = less;
    }

	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t)
            t = bot.GetHuntTarget();
		if (t)
        {
            if ( m_LessThan )
			    return t.m_LastPositionSpread <= m_Radius;

            return t.m_LastPositionSpread >= m_Radius;
        }

		return false;
	}
}
