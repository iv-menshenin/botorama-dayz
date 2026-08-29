//! dmBotCondition_ThreatInRange — true when a hostile target is within attack range.
class dmBotCondition_ThreatInRange : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.GetHostileTarget() != null;
	}
}
