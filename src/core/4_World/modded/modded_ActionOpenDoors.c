//! modded ActionOpenDoors / ActionCloseDoors — publish a door open/close noise so
//! bots can hear a player working a door. Vanilla OnEndServer already adds a native
//! NoiseSystem ping on the server (for zombie/animal sensing); we mirror that with
//! our own dmNoiseSystem signal in the same place so bot hearing receives it too.
//! ActionLockedDoors (subclass of ActionOpenDoors) overrides CreatesNoise() to
//! false — the gate keeps a rattled locked door silent.

modded class ActionOpenDoors
{
	override void OnEndServer(ActionData action_data)
	{
		super.OnEndServer(action_data);
		#ifdef SERVER
		if (CreatesNoise())
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] door open");
			#endif
			dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Target.GetObject().GetPosition(), DM_NOISE_DOOR_STRENGTH, dmNoiseType.SOUND);
		}
		#endif
	}
}

modded class ActionCloseDoors
{
	override void OnEndServer(ActionData action_data)
	{
		super.OnEndServer(action_data);
		#ifdef SERVER
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] door close");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Target.GetObject().GetPosition(), DM_NOISE_DOOR_STRENGTH, dmNoiseType.SOUND);
		#endif
	}
}
