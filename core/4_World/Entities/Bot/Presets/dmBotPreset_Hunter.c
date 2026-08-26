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

		fsm.AddState(idle, "Idle");
		fsm.AddState(hunting, "Hunting");
		fsm.AddState(stealth, "Stealth");
		fsm.AddState(fighting, "Fighting");
		fsm.AddState(surrender, "Surrender");

		//! Вероятности (веса относительные).
		idle.AddTransition(hunting, 0.7);
		idle.AddTransition(stealth, 0.5);
		idle.AddTransition(idle, 0.3);

		//! Требование: сдаться — только при низком здоровье И без патронов.
		idle.AddTransition(surrender, 0.2).Require(dmBotConditions.LowHealth().And(dmBotConditions.NoAmmo()));

		//! Блок: не в бой, если низкое здоровье ИЛИ нет патронов.
		hunting.AddTransition(fighting, 0.6).BlockWhen(dmBotConditions.LowHealth().Or(dmBotConditions.NoAmmo()));

		hunting.AddTransition(idle, 0.4);
		fighting.AddTransition(idle, 1.0);
		stealth.AddTransition(idle, 1.0);
		surrender.AddTransition(idle, 1.0);

		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}
