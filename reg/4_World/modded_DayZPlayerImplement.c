//! modded DayZPlayerImplement — publish a footstep noise so bots can hear other
//! players walking. Vanilla OnStepEvent already adds a native NoiseSystem ping on
//! the server (for zombie/animal sensing); we mirror that with our own dmNoiseSystem
//! signal in the same place so bot hearing receives it too.
modded class DayZPlayerImplement
{
	override void OnStepEvent(string pEventType, string pUserString, int pUserInt)
	{
		super.OnStepEvent(pEventType, pUserString, pUserInt);
		#ifdef SERVER
		dmNoiseSystem.AddNoise(this, GetPosition(), DM_NOISE_STEP_STRENGTH);
		#endif
	}
}
