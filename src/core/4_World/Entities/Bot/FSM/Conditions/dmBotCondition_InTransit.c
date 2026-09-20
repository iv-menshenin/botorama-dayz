//! dmBotCondition_InTransit — true when the bot is travelling between locations.
class dmBotCondition_InTransit : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.GetExplorer().IsInTransit();
	}
}
