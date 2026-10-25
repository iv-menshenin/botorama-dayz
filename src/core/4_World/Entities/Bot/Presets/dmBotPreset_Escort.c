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
		dmBotState shoot  = new dmBotState_Shooting();
		dmBotState flank  = new dmBotState_Flanking();
		dmBotState medical = new dmBotState_MedicalCare();

		fsm.AddState(follow, "Follow");
		fsm.AddState(idle, "Idle");
		fsm.AddState(fight, "Fighting");
		fsm.AddState(shoot, "Shooting");
		fsm.AddState(flank, "Flanking");
		fsm.AddState(medical, "MedicalCare");

		//! Priority weights: >1.0 wins deterministically over normal (<1.0) edges.
		idle.AddTransition(follow, 0.5).Require(dmBotConditions.FollowFar()).BlockWhen(dmBotConditions.HasHostile());
		idle.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		idle.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		idle.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		shoot.AddTransition(idle, 1.0);
		shoot.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		shoot.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		flank.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		flank.AddTransition(idle, 1.0);
		flank.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		fight.AddTransition(idle, 1.0);
		fight.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		follow.AddTransition(shoot, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootVisible()));
		follow.AddTransition(flank, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShootHidden()));
		follow.AddTransition(fight, 2.0).Require(dmBotConditions.HasHostile().And(dmBotConditions.CanShoot().Not()));
		follow.AddTransition(idle, 1.0).BlockWhen(dmBotConditions.HasHostile());
		follow.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		medical.AddTransition(shoot, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShootVisible()));
		medical.AddTransition(flank, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShootHidden()));
		medical.AddTransition(fight, 2.0).Require(dmBotConditions.HasImminentThreat().And(dmBotConditions.CanShoot().Not()));

		fsm.SetDefaultState("Follow");
		fsm.Start();
		return fsm;
	}
}
