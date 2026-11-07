//! modded DayZPlayerImplement — publish a footstep noise so bots can hear other
//! players walking. Vanilla OnStepEvent already adds a native NoiseSystem ping on
//! the server (for zombie/animal sensing); we mirror that with our own dmNoiseSystem
//! signal in the same place so bot hearing receives it too.
modded class DayZPlayerImplement
{
	//! Server time (ms) of the last VON noise we published — throttles voice noise
	//! to once per second (mirrors vanilla m_NextVoNNoiseTime).
	int dm_LastVoiceNoiseTime;

	override void OnStepEvent(string pEventType, string pUserString, int pUserInt)
	{
		super.OnStepEvent(pEventType, pUserString, pUserInt);
		#ifdef SERVER
		//! Сила шума шагов: ЛЁЖА — неслышно; СИДЯ — /3; темп: walk 5 / run 10 / sprint 15.
		HumanMovementState state = new HumanMovementState();
		GetMovementState(state);

		if (state.m_iStanceIdx == DayZPlayerConstants.STANCEIDX_PRONE || state.m_iStanceIdx == DayZPlayerConstants.STANCEIDX_RAISEDPRONE)
			return;   // лёжа — неслышно

		float strength;
		if (state.m_iMovement == DayZPlayerConstants.MOVEMENTIDX_SPRINT)
			strength = DM_NOISE_STEP_SPRINT;
		else if (state.m_iMovement == DayZPlayerConstants.MOVEMENTIDX_RUN)
			strength = DM_NOISE_STEP_RUN;
		else
			strength = DM_NOISE_STEP_WALK;

		if (state.m_iStanceIdx == DayZPlayerConstants.STANCEIDX_CROUCH || state.m_iStanceIdx == DayZPlayerConstants.STANCEIDX_RAISEDCROUCH)
			strength = strength / 3.0;

		dmNoiseSystem.AddNoise(this, GetPosition(), strength, dmNoiseType.STEP);
		#endif
	}

	//! Publish a VON (voice) noise so bots can hear a player talking. Vanilla
	//! CommandHandler already adds a native NoiseSystem ping once per second on the
	//! server for a speaking player; we mirror that with our own dmNoiseSystem signal
	//! (whisper/talk/shout strength). AI bots never speak (IsPlayerSpeaking()==0),
	//! so they generate no self-noise through this hook.
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);
		#ifdef SERVER
		if (IsPlayerSpeaking() > 0.1)
		{
			int now = g_Game.GetTime();
			if (now >= dm_LastVoiceNoiseTime + 1000)
			{
				dm_LastVoiceNoiseTime = now;
				float strength = DM_NOISE_VOICE_TALK;
				int level = g_Game.GetVoiceLevel(this);
				if (level == VoiceLevelWhisper)
					strength = DM_NOISE_VOICE_WHISPER;
				else if (level == VoiceLevelShout)
					strength = DM_NOISE_VOICE_SHOUT;
				#ifdef DM_PERCEPTION_DEBUG
				dmBotLog.Debug("", "[Noise] voice");
				#endif
				dmNoiseSystem.AddNoise(this, GetPosition(), strength, dmNoiseType.SOUND);
			}
		}
		#endif
	}

	//! Shadow the global native GetHive() so PlayerBase.EEKilled's
	//! GetHive().CharacterKill() is skipped for AI bots (no character id).
	Hive GetHive()
	{
		if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
			return null;
		return dmBotGlobalGetHive();
	}
}
