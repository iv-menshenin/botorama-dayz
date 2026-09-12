//! dmBotPreset_Hunter — demo FSM: idle -> hunting/stealth -> fighting/surrender.
//!
//! Shows weighted transitions plus block/require conditions:
//!  - idle -> hunting/stealth decided by CanEnter() (signs / health).
//!  - idle -> surrender requires low health AND no ammo.
//!  - hunting -> fighting blocked when low health OR no ammo.

class dmBotPreset_Hunter
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);

		dmBotState idle      = new dmBotState_Idle();
		dmBotState hunting   = new dmBotState_Hunting();
		dmBotState stealth   = new dmBotState_Stealth();
		dmBotState fighting  = new dmBotState_Fighting();
		dmBotState surrender = new dmBotState_Surrender();
		dmBotState medical   = new dmBotState_MedicalCare();

		fsm.AddState(idle, "Idle");
		fsm.AddState(hunting, "Hunting");
		fsm.AddState(stealth, "Stealth");
		fsm.AddState(fighting, "Fighting");
		fsm.AddState(surrender, "Surrender");
		fsm.AddState(medical, "MedicalCare");

		//! Вероятности (веса относительные).
		idle.AddTransition(hunting, 0.7);
		idle.AddTransition(stealth, 0.5);
		idle.AddTransition(idle, 0.3);
		idle.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());

		//! Требование: сдаться — только при низком здоровье И без патронов.
		idle.AddTransition(surrender, 0.2).Require(dmBotConditions.LowHealth().And(dmBotConditions.NoAmmo()));

		//! Блок: не в бой, если низкое здоровье ИЛИ нет патронов.
		hunting.AddTransition(fighting, 0.6).BlockWhen(dmBotConditions.LowHealth().Or(dmBotConditions.NoAmmo()));

		hunting.AddTransition(idle, 0.4);
		hunting.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		fighting.AddTransition(idle, 1.0);
		stealth.AddTransition(idle, 1.0);
		stealth.AddTransition(medical, 2.0).Require(dmBotConditions.MedicalCare());
		surrender.AddTransition(idle, 1.0);
		medical.AddTransition(fighting, 2.0).Require(dmBotConditions.HasHostile());

		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}
