modded class ZombieBase
{
	void ZombieBase()
	{
		dmEntityRegistry.RegisterZombie(this);
	}

	void ~ZombieBase()
	{
		dmEntityRegistry.UnregisterZombie(this);
	}
}
