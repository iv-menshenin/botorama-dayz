//! dmBotorama — client-side init (logs the loaded mod version).

modded class MissionGameplay
{
	override void OnInit()
	{
		super.OnInit();

		dmBotLog.LogVersion();
	}
}
