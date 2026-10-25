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
		dmBotState flank   = new dmBotState_Flanking();
		dmBotState kamping = new dmBotState_Kamping();
		dmBotState rushing = new dmBotState_Rushing();
		dmBotState explore = new dmBotState_Exploration();
		dmBotState stealth = new dmBotState_Stealth();
		dmBotState medical = new dmBotState_MedicalCare();

		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(flank, "Flanking");
		fsm.AddState(kamping, "Kamping");
		fsm.AddState(rushing, "Rushing");
		fsm.AddState(explore, "Exploration");
		fsm.AddState(stealth, "Stealth");
		fsm.AddState(medical, "MedicalCare");

		idle.AddTransition(explore, 1.0);
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		idle.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		idle.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		idle.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		shoot.AddTransition(idle, 0.1);
		shoot.AddTransition(explore, 1.0);
		shoot.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		shoot.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		shoot.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		shoot.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		flank.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		flank.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		flank.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		flank.AddTransition(idle, 0.1);
		flank.AddTransition(explore, 1.0);
		flank.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		kamping.AddTransition(shoot, 2.0).Require(dmBotConditions.CanShootVisible());
		kamping.AddTransition(flank, 2.0).Require(dmBotConditions.Flanking());
		kamping.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		kamping.AddTransition(idle, 0.1);
		kamping.AddTransition(explore, 1.0);
		kamping.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		rushing.AddTransition(flank, 2.0).Require(dmBotConditions.Flanking());
		rushing.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		rushing.AddTransition(idle, 0.1);
		rushing.AddTransition(explore, 1.0);
		rushing.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		fight.AddTransition(idle, 1.0);
		fight.AddTransition(explore, 1.0);
		fight.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		explore.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		explore.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		explore.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		explore.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		explore.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		explore.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		stealth.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		
		medical.AddTransition(shoot, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShootVisible()));
		medical.AddTransition(flank, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.Flanking()));
		medical.AddTransition(kamping, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.Kamping()));
		medical.AddTransition(rushing, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.Rushing()));
		medical.AddTransition(fight, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShoot().Not()));

		fsm.SetDefaultState("Exploration");
		fsm.Start();
		return fsm;
	}
}
