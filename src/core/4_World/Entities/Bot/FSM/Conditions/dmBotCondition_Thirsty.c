//! dmBotCondition_Thirsty — true when the bot's water is below the travel threshold.
class dmBotCondition_Thirsty : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;
		return pawn.GetStatWater().Get() < DM_TRAVEL_WATER_THRESHOLD;
	}
}
