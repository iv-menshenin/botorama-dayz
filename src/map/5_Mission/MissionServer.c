//! dmBotorama — server-side entry point.

modded class MissionServer
{
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		//! Spawn manager: keep the per-settlement bot population topped up.
		dmBotSpawnManager.Get().Tick(timeslice);

		//! Whole-map road discovery: no-op unless DM_BOT_DISCOVERY is defined.
		dmRoadDiscoveryManager.Get().Tick(timeslice);

		//! Runtime road graph (full + simplified): load + CSR build on first tick.
		dmRoadGraphManager.Get().Tick(timeslice);
	}
}
