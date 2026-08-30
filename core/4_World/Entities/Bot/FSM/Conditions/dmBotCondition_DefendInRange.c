//! dmBotCondition_DefendInRange — true when a hostile target is right on the bot
//! (within DM_DEFEND_RANGE), i.e. the bot should stop following and fight back.
class dmBotCondition_DefendInRange : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.GetDefendTarget() != null;
	}
}
