modded class House
{
	void House()
	{
		#ifdef SERVER
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(dmHouse_Register, DM_HOUSE_REGISTER_DELAY_MS, false);
		#endif
	}

	void dmHouse_Register()
	{
		#ifdef SERVER
		if (GetPosition() != vector.Zero)
			dmLiveBuildingRegistry.Get().Register(this);
		#endif
	}

	void ~House()
	{
		#ifdef SERVER
		dmLiveBuildingRegistry.Get().Unregister(this);
		#endif
	}
}
