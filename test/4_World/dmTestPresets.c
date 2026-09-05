class dmBotTestPreset_Combat
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState fight = new dmBotState_Fighting();
		dmBotState shoot = new dmBotState_Shooting();
		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		shoot.AddTransition(idle, 1.0);
		fight.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}

class dmBotTestPreset_Shooting
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState shoot = new dmBotState_Shooting();
		fsm.AddState(idle, "Idle");
		fsm.AddState(shoot, "Shooting");
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile());
		shoot.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}