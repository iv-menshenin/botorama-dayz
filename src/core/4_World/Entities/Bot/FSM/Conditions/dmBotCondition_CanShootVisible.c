//! dmBotCondition_CanShootVisible — true when the bot can shoot (loaded firearm,
//! non-zombie hostile) AND the current hostile target is visible (m_HasLOS).
//! Equivalent to HasHostile && CanShoot && TargetVisible, as a single leaf so the
//! FSM transitions stay at a 2-level fluent chain (deep .And() nesting segfaults).
class dmBotCondition_CanShootVisible : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t)
			return false;

		EntityAI target = t.m_Entity;
		if (target && target.IsInherited(ZombieBase))
			return false;

		if (!bot.HasLoadedFirearm())
			return false;

		if (!t.m_HasLOS)
			return false;

		return true;
	}
}
