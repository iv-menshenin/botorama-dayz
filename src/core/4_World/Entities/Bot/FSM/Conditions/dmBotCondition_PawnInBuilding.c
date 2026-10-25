//! dmBotCondition_PawnInBuilding — true when the bot itself is inside/under a
//! building (ceiling above the bot's position). Raycast-up + House check; not
//! IsSoundInsideBuilding() (acoustic, unconfirmed on AI_SERVER).
class dmBotCondition_PawnInBuilding : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;
		return dmAISurvivorBase.IsPointUnderRoofBuilding(pawn.GetPosition());
	}
}
