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

		if ( !bot.GetPawn() ) return false;

		// TODO возможно, это справедливо для Follow но не для других состояний
		PlayerBase p = PlayerBase.Cast(t);
		if ( p )
		{
			bool botInVehicle;
			bool targetInVehicle;
			if ( bot.GetPawn().GetCommand_Vehicle() ) botInVehicle = true;
			if ( p.GetCommand_Vehicle() ) targetInVehicle = true;
			if ( targetInVehicle != botInVehicle ) return true;
		}

		vector targetPos = t.GetPosition();
		vector botPos = bot.GetPosition();
		vector d = targetPos - botPos;
		d[1] = 0.0;
		float dist = d.Length();
		float threshold = dmBotState_Follow.GetThresholdDistance(t);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] FollowFar: target=" + t.GetType() + " tPos=" + targetPos + " botPos=" + botPos + " isPawn=" + (t == bot.GetPawn()) + " dist=" + dist + " threshold=" + threshold + " -> " + (dist > threshold));
		#endif

		return dist > threshold;
	}
}
