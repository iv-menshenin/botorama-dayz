//! dmBotCondition_InSandstorm — true when the bot is geographically inside the
//! sandstorm front (intensity > DM_SANDSTORM_ENTER_INTENSITY). Gates entry into
//! the Shelter state (high threshold = hysteresis: exit uses a lower threshold).
class dmBotCondition_InSandstorm : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return dmSandstorm.IsPointInStorm(bot.GetPosition(), DM_SANDSTORM_ENTER_INTENSITY);
	}
}
