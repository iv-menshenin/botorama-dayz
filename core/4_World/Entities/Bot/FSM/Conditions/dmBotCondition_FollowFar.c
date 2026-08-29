//! dmBotCondition_FollowFar — true when the follow target is farther than the
//! follow threshold (i.e. the bot should be in Follow, not Idle).
class dmBotCondition_FollowFar : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		EntityAI t = bot.GetFollowTarget();
		if (!t)
			return false;
		vector d = t.GetPosition() - bot.GetPosition();
		d[1] = 0.0;
		return d.Length() > dmBotState_Follow.GetThresholdDistance(t);
	}
}
