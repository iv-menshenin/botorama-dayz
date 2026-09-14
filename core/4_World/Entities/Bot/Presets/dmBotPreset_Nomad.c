//! dmBotPreset_Nomad — кочевник: скитается (Exploration), ходит к колодцу (Travel)
//! при жажде и защищается при угрозе (Shooting/Fighting/MedicalCare).
class dmBotPreset_Nomad
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);

		dmBotState idle    = new dmBotState_Idle();
		dmBotState travel  = new dmBotState_Travel();
		dmBotState explore = new dmBotState_Exploration();
		dmBotState fight   = new dmBotState_Fighting();
		dmBotState shoot   = new dmBotState_Shooting();
		dmBotState medical = new dmBotState_MedicalCare();

		fsm.AddState(idle, "Idle");
		fsm.AddState(travel, "Travel");
		fsm.AddState(explore, "Exploration");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(medical, "MedicalCare");

		idle.AddTransition(explore, 1.0);
		idle.AddTransition(travel, 1.0);
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		explore.AddTransition(travel, 1.0);
		explore.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		explore.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		explore.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		travel.AddTransition(explore, 1.0);
		travel.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		travel.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));

		shoot.AddTransition(idle, 0.1);
		fight.AddTransition(idle, 0.1);
		shoot.AddTransition(explore, 1.0);
		fight.AddTransition(explore, 1.0);
		medical.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot()));
		medical.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		medical.AddTransition(explore, 1.0);

		fsm.SetDefaultState("Exploration");
		fsm.Start();
		return fsm;
	}
}
