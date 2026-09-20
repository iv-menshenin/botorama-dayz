//! dmLootingConfig — schema of the looting.json (loot/exploration settings).
//!
//! Fields serialize by exact name (no "m_" prefix), matching the JSON keys.
//! Defaults live in Defaults() because JsonSerializer does not apply field
//! initializers when reading.

//! Настройки исследования локации (dmBotState_Exploration): условия выхода.
class dmExplorationConfig
{
	int ExitVisitedCount;          // 0=выкл; >0 — выйти после N посещённых зданий (тест-хук)
	float ExitVisitedPercent;      // 0=выкл; выйти, когда visited/total > N
	float ExitVisitedPercentTime;  // 0=выкл; выйти, когда visited/total > N И time > ниже
	float ExitTimePercentSeconds;  // секунды, парные к ExitVisitedPercentTime
	float ExitTimeSeconds;         // 0=выкл; выйти, когда время в локации > N
}

//! Корень looting.json.
class dmLootingConfig : dmJsonConfigBase
{
	static const int VERSION = 1;

	ref dmExplorationConfig Exploration;

	override void Defaults()
	{
		Version = VERSION;
		Exploration = new dmExplorationConfig();
		Exploration.ExitVisitedCount = 0;
		Exploration.ExitVisitedPercent = 0.9;
		Exploration.ExitVisitedPercentTime = 0.5;
		Exploration.ExitTimePercentSeconds = 1800.0;
		Exploration.ExitTimeSeconds = 3600.0;
	}
}
