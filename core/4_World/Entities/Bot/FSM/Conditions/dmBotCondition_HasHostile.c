//! dmBotCondition_HasHostile — true when a hostile target exists (threat above
//! DM_ATTACK_THREAT_THRESHOLD, alive, not friendly; no distance limit).
class dmBotCondition_HasHostile : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.GetHostileTarget() != null;
	}
}
