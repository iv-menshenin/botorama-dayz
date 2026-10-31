//! dmSpawnConfig — schema of the spawn.json (bot spawn distribution settings).
//!
//! Fields serialize by exact name (no "m_" prefix), matching the JSON keys.
//! Defaults live in Defaults() because JsonSerializer does not apply field
//! initializers when reading.

//! Override количества ботов для конкретной локации.
class dmSpawnSettlement
{
	string Name;   // имя локации для override
	int Count;     // сколько ботов в ней (переопределяет BotsPerSettlement)
}

//! Корень spawn.json.
class dmSpawnConfig : dmJsonConfigBase
{
	static const int VERSION = 1;

	bool Enabled;
	int BotsPerSettlement;
	int MaxBots;
	string SpawnLoadout;
	float RespawnDelay;
	float StartWater;   // стартовый уровень жажды (0..5000, 0 = не менять дефолт спавна)
	float StartEnergy;  // стартовый уровень сытости (0..5000, 0 = не менять дефолт спавна)
	ref array<ref dmSpawnSettlement> Settlements;

	override void Defaults()
	{
		Version = VERSION;
		Enabled = true;
		BotsPerSettlement = 1;
		MaxBots = 40;
		SpawnLoadout = "SurvivorLoadout";
		RespawnDelay = 300.0;
		StartWater = 0.0;
		StartEnergy = 0.0;
		Settlements = new array<ref dmSpawnSettlement>();
	}
}
