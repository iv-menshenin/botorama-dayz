//! dmBotState_Hunting — track down player signs.
class dmBotState_Hunting : dmBotState
{
	override bool CanEnter()
	{
		//! Выслеживать имеет смысл: есть следы И здоровье не критично.
		return GetOwner().HasPlayerSigns() && !GetOwner().IsLowHealth();
	}

	override void OnEntry(dmBotState from)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("[FSM] Hunting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		return EXIT;
	}
}
