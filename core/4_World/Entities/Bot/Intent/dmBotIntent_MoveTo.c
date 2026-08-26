//! dmBotIntent_MoveTo — walk to a world point; finishes when reached.
class dmBotIntent_MoveTo : dmBotIntent
{
	vector m_Target;
	float m_ReachDistance = 0.5;

	override void OnStart(dmAISurvivor bot)
	{
		bot.FacePoint(m_Target);
		bot.SetWalk(true);
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		bot.SetWalk(true);   // re-assert (movement reset each tick before arbitration)
		float dist = vector.Distance(bot.GetPosition(), m_Target);
		if (dist <= m_ReachDistance)
		{
			bot.SetWalk(false);
			Finish();
		}
	}

	override void OnCancel(dmAISurvivor bot)
	{
		bot.SetWalk(false);
	}
}
