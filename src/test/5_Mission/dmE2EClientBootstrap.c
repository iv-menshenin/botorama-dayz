//! dmE2EClientBootstrap — kicks the CLIENT-side e2e bridge from the client mission.
//!
//! MissionGameplay is the CLIENT mission in MP (MissionServer is the server one),
//! so a modded class MissionGameplay here runs only on the observer client. The
//! bridge self-schedules its CallLater tick — it does NOT ride OnUpdate (1.30:
//! two modded MissionGameplay blocks both overriding OnUpdate silently drop one).
//!
//! We hook OnMissionStart (NOT OnInit, which core already overrides in
//! src/core/5_Mission/MissionGameplay.c) so the two modded blocks do not collide:
//! they override DIFFERENT methods. OnMissionStart fires once after load
//! (SetMissionState(MISSION_STATE_GAME)) — the right place to start a per-tick
//! CallLater on the client.

modded class MissionGameplay
{
	override void OnMissionStart()
	{
		super.OnMissionStart();

		dmE2EClientBridge.Get();
	}
}
