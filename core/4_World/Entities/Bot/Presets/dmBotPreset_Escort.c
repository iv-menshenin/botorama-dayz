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

		idle.AddTransition(follow, 1.0).Require(dmBotConditions.FollowFar());
		idle.AddTransition(fight, 1.0).Require(dmBotConditions.ThreatInRange());
		fight.AddTransition(idle, 1.0);
		follow.AddTransition(fight, 1.0).Require(dmBotConditions.ThreatInRange());
		follow.AddTransition(idle, 1.0).BlockWhen(dmBotConditions.ThreatInRange());

		fsm.SetDefaultState("Follow");
		fsm.Start();
		return fsm;
	}
}
