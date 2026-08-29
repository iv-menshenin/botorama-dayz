//! dmBotPreset_Escort — FSM for escorting the player: follow alongside, or idle
//! when there is nobody to follow.
class dmBotPreset_Escort
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);

		dmBotState follow = new dmBotState_Follow();
		dmBotState idle   = new dmBotState_Idle();

		fsm.AddState(follow, "Follow");
		fsm.AddState(idle, "Idle");

		follow.AddTransition(idle, 1.0);
		idle.AddTransition(follow, 1.0);

		fsm.SetDefaultState("Follow");
		fsm.Start();
		return fsm;
	}
}
