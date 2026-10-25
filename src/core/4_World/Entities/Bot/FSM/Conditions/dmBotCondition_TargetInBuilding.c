//! dmBotCondition_TargetInBuilding — true when the current hostile target is
//! inside/under a building (ceiling above the target's position).
class dmBotCondition_TargetInBuilding : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t || !t.m_Entity)
			return false;
		return dmAISurvivorBase.IsPointUnderRoofBuilding(t.m_Entity.GetPosition());
	}
}
