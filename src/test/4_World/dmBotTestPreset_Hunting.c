//! dmBotTestPreset_Hunting — минимальный тест-пресет «только Hunting»: Idle + Hunting.
//! Вход в Hunting гейтит CanEnter() (GetHuntTarget() != null); возврат в Idle по весу.
class dmBotTestPreset_Hunting
{
	static dmBotFSM Create(dmAISurvivor owner)
	{
		dmBotFSM fsm = new dmBotFSM(owner);
		dmBotState idle = new dmBotState_Idle();
		dmBotState hunting = new dmBotState_Hunting();
		fsm.AddState(idle, "Idle");
		fsm.AddState(hunting, "Hunting");
		idle.AddTransition(hunting, 1.0);
		hunting.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		return fsm;
	}
}
