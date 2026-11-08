//! dmBotCondition_HasVisibleHostile — true when there is a strictly VISIBLE hostile
//! target (GetHostileTargetEx(true), no fallback to a non-visible target). Used to
//! gate Shelter entry (don't shelter while fighting) and exit (hostile preempts).
class dmBotCondition_HasVisibleHostile : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.GetHostileTargetEx(true) != null;
	}
}
