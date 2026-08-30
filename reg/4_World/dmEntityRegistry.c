//! dmEntityRegistry — global registry of world entities (zombies, animals, players).
//!
//! Replaces the per-bot spatial box query in dmVision: entities self-register in
//! their constructor and unregister in their destructor. Dead entities are removed
//! lazily by Cleanup() (IsAlive() check) so the registry never grows unbounded.
//!
//! Entities are engine-managed classes, so the element type carries no `ref` (see
//! codeguide: managed classes use references without `ref`). The `autoptr` storage
//! on the arrays auto-initializes them to empty (Expansion eAIPatrol pattern).

class dmEntityRegistry
{
	static autoptr array<ZombieBase> s_Zombies = new array<ZombieBase>;
	static autoptr array<AnimalBase> s_Animals = new array<AnimalBase>;
	static autoptr array<PlayerBase> s_Players = new array<PlayerBase>;

	static void RegisterZombie(ZombieBase e) { s_Zombies.Insert(e); }
	static void UnregisterZombie(ZombieBase e) { s_Zombies.RemoveItem(e); }
	static void RegisterAnimal(AnimalBase e) { s_Animals.Insert(e); }
	static void UnregisterAnimal(AnimalBase e) { s_Animals.RemoveItem(e); }
	static void RegisterPlayer(PlayerBase e) { s_Players.Insert(e); }
	static void UnregisterPlayer(PlayerBase e) { s_Players.RemoveItem(e); }

	static array<ZombieBase> GetZombies() { return s_Zombies; }
	static array<AnimalBase> GetAnimals() { return s_Animals; }
	static array<PlayerBase> GetPlayers() { return s_Players; }

	//! Remove stale entries: null (deleted entities) and dead (IsAlive() == false).
	//! Players are NOT dropped on IsAlive — a player "death" is a corpse state, not
	//! a removal; only null (deleted) players are dropped.
	static void Cleanup()
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Registry.Cleanup");
		#endif

		int i;
		for (i = s_Zombies.Count() - 1; i >= 0; i--)
		{
			if (!s_Zombies[i] || !s_Zombies[i].IsAlive())
				s_Zombies.Remove(i);
		}
		for (i = s_Animals.Count() - 1; i >= 0; i--)
		{
			if (!s_Animals[i] || !s_Animals[i].IsAlive())
				s_Animals.Remove(i);
		}
		for (i = s_Players.Count() - 1; i >= 0; i--)
		{
			if (!s_Players[i])
				s_Players.Remove(i);
		}
	}
}
