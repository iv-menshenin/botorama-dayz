//! dmBotCondition_NothingToDo — true when the explorer has nothing left to do.
class dmBotCondition_NothingToDo : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.GetExplorer().IsNothingToDo();
	}
}
