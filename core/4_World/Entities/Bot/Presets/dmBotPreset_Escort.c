//! dmBotPreset_Escort — FSM for escorting an entity: Follow (PREEMPTIVE) when the
//! target is far, Idle (INTERRUPTIBLE) when it is close.
class dmBotPreset_Escort
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);

		dmBotState follow = new dmBotState_Follow();
		dmBotState idle   = new dmBotState_Idle();

		fsm.AddState(follow, "Follow");
		fsm.AddState(idle, "Idle");

		idle.AddTransition(follow, 1.0).Require(dmBotConditions.FollowFar());
		follow.AddTransition(idle, 1.0);

		fsm.SetDefaultState("Follow");
		fsm.Start();
		return fsm;
	}
}
