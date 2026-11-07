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

	//! Shadow the global native GetHive() so PlayerBase.EEKilled's
	//! GetHive().CharacterKill() is skipped for AI bots (no character id).
	Hive GetHive()
	{
		if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
			return null;
		return dmBotGlobalGetHive();
	}
}
