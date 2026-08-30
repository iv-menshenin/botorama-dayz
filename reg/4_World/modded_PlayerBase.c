modded class PlayerBase
{
	void PlayerBase()
	{
		dmEntityRegistry.RegisterPlayer(this);
	}

	void ~PlayerBase()
	{
		dmEntityRegistry.UnregisterPlayer(this);
	}
}
