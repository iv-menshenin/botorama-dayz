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
		dmBotState shoot = new dmBotState_Shooting();
		dmBotState flank = new dmBotState_Flanking();
		dmBotState kamping = new dmBotState_Kamping();
		dmBotState rushing = new dmBotState_Rushing();
		dmBotState medical = new dmBotState_MedicalCare();
		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(flank, "Flanking");
		fsm.AddState(kamping, "Kamping");
		fsm.AddState(rushing, "Rushing");
		fsm.AddState(medical, "MedicalCare");
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		idle.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		idle.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		idle.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		shoot.AddTransition(idle, 1.0);
		shoot.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		shoot.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.Flanking()));
		shoot.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		shoot.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		flank.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		flank.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		flank.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		flank.AddTransition(idle, 1.0);
		flank.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		kamping.AddTransition(shoot, 2.0).Require(dmBotConditions.CanShootVisible());
		kamping.AddTransition(flank, 2.0).Require(dmBotConditions.Flanking());
		kamping.AddTransition(rushing, 2.0).Require(dmBotConditions.Rushing());
		kamping.AddTransition(idle, 1.0);
		kamping.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		rushing.AddTransition(flank, 2.0).Require(dmBotConditions.Flanking());
		rushing.AddTransition(kamping, 2.0).Require(dmBotConditions.Kamping());
		rushing.AddTransition(idle, 1.0);
		rushing.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		fight.AddTransition(idle, 1.0);
		fight.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		medical.AddTransition(shoot, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShootVisible()));
		medical.AddTransition(flank, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.Flanking()));
		medical.AddTransition(kamping, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.Kamping()));
		medical.AddTransition(rushing, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.Rushing()));
		medical.AddTransition(fight, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShoot().Not()));
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}
