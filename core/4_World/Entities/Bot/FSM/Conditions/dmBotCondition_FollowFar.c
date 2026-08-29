//! dmBotCondition_FollowFar — true when the follow target is farther than the
//! follow threshold (i.e. the bot should be in Follow, not Idle).
class dmBotCondition_FollowFar : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		EntityAI t = bot.GetFollowTarget();
		if (!t)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] FollowFar: нет цели следования");
			#endif
			return false;
		}

		vector d = t.GetPosition() - bot.GetPosition();
		d[1] = 0.0;
		float dist = d.Length();
		float threshold = dmBotState_Follow.GetThresholdDistance(t);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] FollowFar: target=" + t.GetType() + " dist=" + dist + " threshold=" + threshold + " -> " + (dist > threshold));
		#endif

		return dist > threshold;
	}
}
