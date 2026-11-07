//! modded ActionMineTree / ActionDigWorms / ActionMineRock — publish a gathering
//! noise (chopping wood / digging worms / mining rock) so bots can hear a player
//! working nearby. Vanilla adds no such signal on the server; we emit our own in
//! OnFinishProgressServer, which fires once per material drop / dig cycle (not per
//! swing — see docs/research/noise-actions-hooks.md §1.4).

modded class ActionMineTree
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);
		#ifdef SERVER
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] gather");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_STRENGTH, dmNoiseType.SOUND);
		#endif
	}
}

modded class ActionDigWorms
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);
		#ifdef SERVER
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] gather");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_STRENGTH, dmNoiseType.SOUND);
		#endif
	}
}

modded class ActionMineRock
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);
		#ifdef SERVER
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] gather");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_STRENGTH, dmNoiseType.SOUND);
		#endif
	}
}
