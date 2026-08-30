//! dmBotPreset_Escort — FSM for escorting an entity: Follow (PREEMPTIVE) when the
//! target is far, Idle (INTERRUPTIBLE) when it is close.
class dmBotPreset_Escort
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);

		dmBotState follow = new dmBotState_Follow();
		dmBotState idle   = new dmBotState_Idle();
		dmBotState fight  = new dmBotState_Fighting();

		fsm.AddState(follow, "Follow");
		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");

		//! Priority weights: >1.0 wins deterministically over normal (<1.0) edges.
		idle.AddTransition(follow, 0.5).Require(dmBotConditions.FollowFar()).BlockWhen(dmBotConditions.HasHostile());
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile());
		fight.AddTransition(idle, 1.0);
		follow.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile());
		follow.AddTransition(idle, 1.0).BlockWhen(dmBotConditions.HasHostile());

		fsm.SetDefaultState("Follow");
		fsm.Start();
		return fsm;
	}
}
