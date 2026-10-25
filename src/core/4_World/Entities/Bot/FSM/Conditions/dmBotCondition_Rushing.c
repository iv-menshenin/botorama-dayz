//! dmBotCondition_Rushing — «catch-all» боевое состояние: есть сущность-цель +
//! не-зомби + заряженный ствол. Не гейтится видимостью/крышей/досягаемостью —
//! спор с Shooting/Flanking/Kamping решается позже через «характер».
class dmBotCondition_Rushing : dmBotCondition
{
	bool m_InHands;

	void dmBotCondition_Rushing(bool inHands = false)
	{
		m_InHands = inHands;
	}

	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t || !t.m_Entity)
			return false;

		EntityAI target = t.m_Entity;
		if (target && target.IsInherited(ZombieBase))
			return false;

		if (!bot.CanShoot(m_InHands))
			return false;

		return true;
	}
}
