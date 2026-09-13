//! dmBotorama — server-side entry point.

modded class MissionServer
{
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		//! Heartbeat for all bots (fixed-rate, accumulated inside TickAll).
		dmAISurvivor.TickAll(timeslice);

		//! Spawn manager: keep the per-settlement bot population topped up.
		dmBotSpawnManager.Get().Tick(timeslice);
	}
}
