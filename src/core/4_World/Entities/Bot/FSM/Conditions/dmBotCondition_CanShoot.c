//! dmBotCondition_CanShoot — true when the bot has a loaded firearm and the target
//! is not a zombie (zombies are engaged with melee). Gates the Shooting state.
class dmBotCondition_CanShoot : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		EntityAI target = null;
		if (t)
			target = t.m_Entity;

		//! Зомби — только мили (не пускаем в Shooting).
		if (target && target.IsInherited(ZombieBase))
			return false;

		return bot.HasLoadedFirearm();
	}
}
