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

class dmBotTestPreset_Medical
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState medical = new dmBotState_MedicalCare();
		fsm.AddState(idle, "Idle");
		fsm.AddState(medical, "MedicalCare");
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		medical.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}

class dmBotTestPreset_Hunting
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState shoot = new dmBotState_Shooting();
		dmBotState hunt = new dmBotState_Hunting();
		dmBotState flank   = new dmBotState_Flanking();
		dmBotState kamping = new dmBotState_Kamping();
		dmBotState rushing = new dmBotState_Rushing();

		fsm.AddState(idle, "Idle");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(hunt, "Hunting");
		fsm.AddState(flank, "Flanking");
		fsm.AddState(kamping, "Kamping");
		fsm.AddState(rushing, "Rushing");

		idle.AddTransition(hunt, 2.0).Require(dmBotConditions.HasUnknownTarget().And(dmBotConditions.LastPositionSpreadGreatOrEqual(15.0)));
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.LastPositionSpreadLessOrEqual(15.0)));
		idle.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		idle.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		idle.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());

		shoot.AddTransition(idle, 1.0);
		flank.AddTransition(idle, 1.0);
		kamping.AddTransition(idle, 1.0);
		rushing.AddTransition(idle, 1.0);

		shoot.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		shoot.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		shoot.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());

		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}

class dmBotTestPreset_Kamping
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState kamping = new dmBotState_Kamping();
		fsm.AddState(idle, "Idle");
		fsm.AddState(kamping, "Kamping");
		idle.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		kamping.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}

class dmBotTestPreset_Flanking
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState flank = new dmBotState_Flanking();
		fsm.AddState(idle, "Idle");
		fsm.AddState(flank, "Flanking");
		idle.AddTransition(flank, 2.0).Require(dmBotConditions.Flanking());
		flank.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}