//! dmBotIntent_GetInVehicle — walk to a vehicle's crew entry point and board it.
//!
//! It inherits the full path-following machinery from dmBotIntent_MoveTo (steering,
//! stuck detection, vault/climb, ladders, recovery) and aims the route at the seat's
//! entry point (Transport.CrewEntryWS). DM_GETIN_REACH is the reach distance at
//! which the entry point counts as reached. OnReachedGoal starts the vanilla
//! vehicle command (pawn.GetInVehicle) and finishes — the vehicle command then owns
//! the body on the engine side; on failure it fails the intent.
class dmBotIntent_GetInVehicle : dmBotIntent_MoveTo
{
	Transport m_Transport;
	int m_Seat = -1;

	void dmBotIntent_GetInVehicle()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "GetInVehicle";
	}

	override void OnStart(dmAISurvivor bot)
	{
		vector entry;
		vector dir;
		if (m_Transport)
			m_Transport.CrewEntryWS(m_Seat, entry, dir);
		else
			entry = bot.GetPosition();
		m_Goal = entry;
		m_ReachDistance = DM_GETIN_REACH;
		super.OnStart(bot);
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !pawn.GetInVehicle(m_Transport, m_Seat))
		{
			Fail();
			return;
		}
		Finish();
	}
}
