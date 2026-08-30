//! dmBotPreset_Combat — melee combat FSM: Idle until a hostile target is in
//! attack range, then Fighting (PREEMPTIVE) until it is gone (Fighting EXITs
//! back to Idle when no hostile target remains).
class dmBotPreset_Combat
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState fight = new dmBotState_Fighting();
		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");
		idle.AddTransition(fight, 1.0).Require(dmBotConditions.HasHostile());
		fight.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}
