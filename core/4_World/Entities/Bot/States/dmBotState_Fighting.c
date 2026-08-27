//! dmBotState_Fighting — engage a target.
class dmBotState_Fighting : dmBotState
{
	override void OnEntry(dmBotState from)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Fighting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		return EXIT;
	}
}
