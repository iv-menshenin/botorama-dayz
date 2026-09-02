//! dmBotPreset_Survivor — FSM for creating behavior that is as close as possible to that of a real player.
//! The bot will collect loot, travel, and defend itself.
class dmBotPreset_Survivor
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);

		dmBotState idle    = new dmBotState_Idle();
		dmBotState fight   = new dmBotState_Fighting();
		dmBotState shoot   = new dmBotState_Shooting();
		dmBotState explore = new dmBotState_Exploration();
		dmBotState stealth = new dmBotState_Stealth();

		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(explore, "Exploration");
		fsm.AddState(stealth, "Stealth");

		idle.AddTransition(explore, 1.0);
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		shoot.AddTransition(idle, 1.0);
		fight.AddTransition(idle, 1.0);
		explore.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		explore.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));

		fsm.SetDefaultState("Exploration");
		fsm.Start();
		return fsm;
	}
}
