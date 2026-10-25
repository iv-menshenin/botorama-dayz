//! dmBotCondition_Rushing — CanShoot && !TargetVisible && !TargetInBuilding &&
//! beyondRange (нет оружия на дистанцию цели — надо сблизиться). Зеркалит
//! dmBotCondition_Flanking, но по обратному признаку досягаемости.
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

		if (t.m_HasLOS)
			return false;

		if (dmAISurvivorBase.IsPointUnderRoofBuilding(target.GetPosition()))
			return false;

		// rush только если НЕТ оружия на эту дистанцию
		if (bot.GetArsenal().WeaponAllowDist(bot.DistanceToTarget(t)).Count() > 0)
			return false;

		return true;
	}
}
