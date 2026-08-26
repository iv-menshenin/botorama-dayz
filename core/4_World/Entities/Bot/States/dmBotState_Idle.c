//! dmBotState_Idle — stand around / look about.
class dmBotState_Idle : dmBotState
{
	override void OnEntry(dmBotState from)
	{
		#ifdef DM_BOT_DEBUG
		string fromName = "null";
		if (from)
			fromName = from.GetName();
		dmBotLog.Debug("[FSM] Idle.entry from=" + fromName);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		//! Demo: exit immediately so transitions churn and the weight distribution
		//! is observable. A real Idle would wait/scan for a while.
		return EXIT;
	}
}
