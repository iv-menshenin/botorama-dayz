//! dmBotCondition_HasPlayerSigns — true when the bot perceives player signs
//! (killed zombie, campfire, player-used items) nearby.
class dmBotCondition_HasPlayerSigns : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.HasPlayerSigns();
	}
}
