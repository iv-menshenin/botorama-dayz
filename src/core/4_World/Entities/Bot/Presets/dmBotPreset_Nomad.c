//! dmBotPreset_Nomad — кочевник: скитается (Exploration), кочует между локациями
//! (Travel) и защищается при угрозе (Shooting/Fighting/MedicalCare).
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
		dmBotState flank   = new dmBotState_Flanking();
		dmBotState medical = new dmBotState_MedicalCare();

		fsm.AddState(idle, "Idle");
		fsm.AddState(travel, "Travel");
		fsm.AddState(explore, "Exploration");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(flank, "Flanking");
		fsm.AddState(medical, "MedicalCare");

		idle.AddTransition(explore, 1.0);
		idle.AddTransition(travel, 1.0);
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		idle.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		explore.AddTransition(travel, 1.0);
		explore.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		explore.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		explore.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		explore.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		explore.AddTransition(idle, 0.1);

		travel.AddTransition(explore, 1.0);
		travel.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		travel.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		travel.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		travel.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		shoot.AddTransition(idle, 0.1);
		fight.AddTransition(idle, 0.1);
		shoot.AddTransition(explore, 1.0);
		fight.AddTransition(explore, 1.0);
		shoot.AddTransition(travel, 1.0);
		fight.AddTransition(travel, 1.0);
		shoot.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		fight.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		shoot.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		flank.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		flank.AddTransition(idle, 0.1);
		flank.AddTransition(explore, 1.0);
		flank.AddTransition(travel, 1.0);
		flank.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		medical.AddTransition(shoot, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShootVisible()));
		medical.AddTransition(flank, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShootHidden()));
		medical.AddTransition(fight, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShoot().Not()));
		medical.AddTransition(explore, 1.0);
		medical.AddTransition(travel, 1.0);

		fsm.SetDefaultState("Exploration");
		fsm.Start();
		return fsm;
	}
}
