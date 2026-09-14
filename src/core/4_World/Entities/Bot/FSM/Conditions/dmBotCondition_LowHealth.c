//! dmBotCondition_LowHealth — true when the bot's health is below the threshold.
class dmBotCondition_LowHealth : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.IsLowHealth();
	}
}
