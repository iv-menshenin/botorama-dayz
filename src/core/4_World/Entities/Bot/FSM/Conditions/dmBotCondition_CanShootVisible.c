//! dmBotCondition_CanShootVisible — true when the bot can shoot (loaded firearm,
//! non-zombie hostile) AND the current hostile target is visible (m_HasLOS).
//! Equivalent to HasHostile && CanShoot && TargetVisible, as a single leaf so the
//! FSM transitions stay at a 2-level fluent chain (deep .And() nesting segfaults).
class dmBotCondition_CanShootVisible : dmBotCondition
{
	bool m_InHands;

	void dmBotCondition_CanShootVisible(bool inHands = false)
	{
		m_InHands = inHands;
	}

	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t)
			return false;

		EntityAI target = t.m_Entity;
		if (target && target.IsInherited(ZombieBase))
			return false;

		if (!bot.CanShoot(m_InHands))
			return false;

		if (!t.IsVisible())
			return false;

		if (!bot.CanReach(t))
			return false;

		return true;
	}
}
