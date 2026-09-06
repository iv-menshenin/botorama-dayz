modded class ZombieBase
{
	void ZombieBase()
	{
		dmEntityRegistry.RegisterZombie(this);
	}

	void ~ZombieBase()
	{
		dmEntityRegistry.UnregisterZombie(this);
	}

	//! Publish a cry/roar noise so bots can hear an infected vocalize. Vanilla
	//! OnSoundVoiceEvent already adds a native NoiseSystem ping on the server for
	//! voice events; we mirror that with our own dmNoiseSystem signal.
	override void OnSoundVoiceEvent(int event_id, string event_user_string)
	{
		super.OnSoundVoiceEvent(event_id, event_user_string);
		#ifdef SERVER
		dmNoiseSystem.AddNoise(this, GetPosition(), DM_NOISE_SCREAM_STRENGTH, dmNoiseType.SCREAM);
		#endif
	}
}
