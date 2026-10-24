
//! dmCarRepair — общая логика починки машины (зоны + attachments + oil/brake).
//! Вынесена в 4_World, чтобы её звали и chat-команда (5_Mission), и E2E-мост (4_World).
class dmCarRepair
{
	static void Repair(CarScript car, out int zonesCount, out int attCount)
	{
		zonesCount = 0;
		attCount = 0;

		array<string> zones = new array<string>();
		car.GetDamageZones(zones);
		int i;
		for (i = 0; i < zones.Count(); i++)
			car.SetHealthMax(zones.Get(i), "Health");
		car.SetHealthMax("", "Health");
		zonesCount = zones.Count();

		attCount = car.GetInventory().AttachmentCount();
		for (i = 0; i < attCount; i++)
		{
			EntityAI att = car.GetInventory().GetAttachmentFromIndex(i);
			if (att)
				att.SetHealthMax("", "Health");
		}

		//! Только oil/brake; FUEL/COOLANT НЕ трогаем.
		if (car.GetFluidFraction(CarFluid.OIL) < 1.0)
			car.Fill(CarFluid.OIL, car.GetFluidCapacity(CarFluid.OIL));
		if (car.GetFluidFraction(CarFluid.BRAKE) < 1.0)
			car.Fill(CarFluid.BRAKE, car.GetFluidCapacity(CarFluid.BRAKE));

		#ifdef DM_BOT_DEBUG_CAR
		dmBotLog.Debug("[CAR] repair: " + car.GetType() + " зоны=" + zonesCount + " детали=" + attCount);
		#endif
	}
}
