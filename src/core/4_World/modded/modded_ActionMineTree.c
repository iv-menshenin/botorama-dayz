//! modded ActionMineTree / ActionDigWorms / ActionMineRock — publish a gathering
//! noise so bots can hear a player working nearby. Vanilla adds no such signal on
//! the server; we emit our own:
//! - ActionMineTree: DROP (felled tree, ~50 m) per material drop (OnFinishProgressServer)
//!   + SOUND (per swing, ~20 m) per swing (OnExecuteServer — HasMultipleExecution=true).
//! - ActionDigWorms: SOUND (per dig, ~20 m) on the process — gated to shovel/field
//!   shovel only (knife/machete digging is quiet).
//! - ActionMineRock: SOUND (mining rock, ~15 m) per material drop (OnFinishProgressServer).

modded class ActionMineTree
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);
		#ifdef SERVER
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] tree drop (felled tree)");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_DROP_STRENGTH, dmNoiseType.SOUND);
		#endif
	}

	override void OnExecuteServer(ActionData action_data)
	{
		super.OnExecuteServer(action_data);
		#ifdef SERVER
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] tree swing (process)");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_PROCESS_STRENGTH, dmNoiseType.SOUND);
		#endif
	}
}

modded class ActionDigWorms
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);
	}

	override void OnExecuteServer(ActionData action_data)
	{
		super.OnExecuteServer(action_data);
		#ifdef SERVER
		if (action_data.m_MainItem && (action_data.m_MainItem.IsInherited(Shovel) || action_data.m_MainItem.IsInherited(FieldShovel)))
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] dig worms (process, shovel)");
			#endif
			dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_PROCESS_STRENGTH, dmNoiseType.SOUND);
		}
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
		dmBotLog.Debug("", "[Noise] mine rock");
		#endif
		dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_GATHER_STRENGTH, dmNoiseType.SOUND);
		#endif
	}
}
