modded class AnimalBase
{
	void AnimalBase()
	{
		dmEntityRegistry.RegisterAnimal(this);
	}

	void ~AnimalBase()
	{
		dmEntityRegistry.UnregisterAnimal(this);
	}
}
