//! dmBotState_Stealth — move covertly (low health).
class dmBotState_Stealth : dmBotState
{
	override bool CanEnter()
	{
		return GetOwner().IsLowHealth();
	}

	override void OnEntry(dmBotState from)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("[FSM] Stealth.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		return EXIT;
	}
}
