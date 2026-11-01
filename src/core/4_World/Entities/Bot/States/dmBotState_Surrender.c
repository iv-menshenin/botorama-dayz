//! dmBotState_Surrender — give up (low health AND no ammo).
class dmBotState_Surrender : dmBotState
{
	override void OnEntry(dmBotState from)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug(GetOwner().GetName(), "[FSM] Surrender.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		return EXIT;
	}
}
