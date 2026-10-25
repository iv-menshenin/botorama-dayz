//! dmBotCondition_Kamping — CanShoot && TargetInBuilding && !TargetVisible &&
//! !PawnInBuilding. Gates the Kamping state (camp a building's exit while the
//! target hides inside). Single leaf: the bot has a loaded firearm, the hostile
//! target is out of sight AND inside a building, and the bot itself is outside.
class dmBotCondition_Kamping : dmBotCondition
{
	bool m_InHands;

	void dmBotCondition_Kamping(bool inHands = false)
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

		if (t.m_HasLOS)
			return false;

		if (!dmAISurvivorBase.IsPointUnderRoofBuilding(target.GetPosition()))
			return false;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn && dmAISurvivorBase.IsPointUnderRoofBuilding(pawn.GetPosition()))
			return false;

		if (!bot.CanReach(t))
			return false;

		return true;
	}
}
