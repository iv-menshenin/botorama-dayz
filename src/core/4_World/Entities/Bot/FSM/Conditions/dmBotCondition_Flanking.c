//! dmBotCondition_Flanking — CanShoot && !TargetVisible && !TargetInBuilding.
//! Gates the Flanking state (arc around cover for a clear shot) as a single leaf
//! so preset transitions stay at a 2-level fluent chain. Replaces the old
//! CanShootHidden: a hidden target inside a building is NOT flanked (that is
//! Kamping's job), so it is excluded here.
class dmBotCondition_Flanking : dmBotCondition
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

		if (t.m_HasLOS)
			return false;

		if (dmAISurvivorBase.IsPointUnderRoofBuilding(target.GetPosition()))
			return false;

		return true;
	}
}
