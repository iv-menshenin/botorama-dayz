//! dmBotIntent_GetOutVehicle — exit the vehicle the bot is currently seated in.
//!
//! EXCLUSIVE + MOVE channel (like OpenDoor/UseLadder): while it runs it blocks
//! parallel movement intents so the get-out animation isn't interrupted. The
//! vanilla vehicle command owns the body during the animation; completion is
//! detected when GetCommand_Vehicle() becomes null (the bot is fully out).
class dmBotIntent_GetOutVehicle : dmBotIntent
{
	float m_Grace = 0.0;

	void dmBotIntent_GetOutVehicle()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "GetOutVehicle";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);
		m_Grace = DM_GETOUT_GRACE;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !pawn.GetOutVehicle())
		{
			Fail();
			return;
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			Fail();
			return;
		}

		if (m_Grace > 0.0)
		{
			m_Grace -= pDt;
			return;
		}

		if (!pawn.GetCommand_Vehicle())
			Finish();
	}
}
