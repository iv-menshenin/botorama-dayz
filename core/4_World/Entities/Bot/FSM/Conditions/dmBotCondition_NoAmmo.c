//! dmBotCondition_NoAmmo — true when the bot has no ammo.
class dmBotCondition_NoAmmo : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.HasNoAmmo();
	}
}
