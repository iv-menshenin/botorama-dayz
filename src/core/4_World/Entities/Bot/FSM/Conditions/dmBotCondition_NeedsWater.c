//! dmBotCondition_NeedsWater — true when the bot is thirsty, has an empty bottle to
//! fill, has nothing to drink right now, is not in combat, and a water source exists
//! within DM_GET_WATER_SEARCH_RADIUS. Gates the GetWater state.
class dmBotCondition_NeedsWater : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		if (bot.IsInCombat())
			return false;

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return false;

		if (pawn.GetStatWater().Get() >= DM_EAT_DRINK_WATER)
			return false;

		//! Есть что выпить прямо сейчас — наполнение не нужно (EatDrink справится сам).
		if (dmLoot.FindEdible(pawn, true) != null)
			return false;

		if (dmLoot.FindEmptyBottle(pawn) == null)
			return false;

		//! Нет источника воды в радиусе поиска — наполнять нечем.
		Building source = dmLiveBuildingRegistry.Get().GetNearestWaterSource(bot.GetPosition(), DM_GET_WATER_SEARCH_RADIUS);
		if (!source)
			return false;

		//! Не идти за водой в бурю: источник внутри фронта — подождём её прохода.
		if (dmSandstorm.IsPointInStorm(source.GetPosition(), DM_SANDSTORM_EXIT_INTENSITY))
			return false;

		return true;
	}
}
