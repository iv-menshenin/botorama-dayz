//! dmBotCondition_Rushing — «catch-all» боевое состояние: есть сущность-цель +
//! не-зомби + заряженный ствол. Не гейтится видимостью/крышей/досягаемостью —
//! спор с Shooting/Flanking/Kamping решается позже через «характер».
class dmBotCondition_Rushing : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t || !t.m_Entity)
			return false;

		EntityAI target = t.m_Entity;
		if (target && target.IsInherited(ZombieBase))
			return false;

		if (!bot.HasLoadedFirearm())
			return false;

		return true;
	}
}
