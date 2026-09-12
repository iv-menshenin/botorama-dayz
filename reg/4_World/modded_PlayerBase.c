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
	
	override bool Save()
	{
		if ( !GetHive() ) return false;

		return super.Save();
	}
}
