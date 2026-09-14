//! dmPoiCommand — команда "/poi": сводка по поселениям/точкам и goto-заглушка.
class dmPoiCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_POI;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
		{
			dmCommandManager.ChatToPlayer(player, "Использование: /poi | /poi goto <тип>");
			return true;
		}

		if (parts[1] == "goto" && parts.Count() >= 3)
			return HandleGoto(player, parts[2]);

		return HandleReport(player);
	}

	//! Сводка: число поселений/точек и первые ~8 поселений (имя, тип, x,z).
	private bool HandleReport(PlayerBase player)
	{
		dmWorldPOIRegistry registry = dmWorldPOIRegistry.Get();
		if (!registry)
		{
			dmCommandManager.ChatToPlayer(player, "Реестр POI недоступен.");
			return true;
		}

		dmCommandManager.ChatToPlayer(player, "Поселений: " + registry.SettlementCount());

		int shown = 0;
		int i;
		for (i = 0; i < registry.SettlementCount(); i++)
		{
			if (shown >= 8)
				break;

			dmWorldPoiLocation settlement = registry.GetSettlement(i);
			if (!settlement)
				continue;

			vector pos = settlement.Position;
			dmCommandManager.ChatToPlayer(player, settlement.Name + " (" + settlement.Type + ") " + (int)pos[0] + "," + (int)pos[2]);
			shown++;
		}

		return true;
	}

	//! Заглушка goto: разбор типа и сообщение. TODO: teleport к точке типа.
	private bool HandleGoto(PlayerBase player, string typeStr)
	{
		dmWorldPOIType type = dmBuildingInteriorMap.ParseType(typeStr);
		if (type == dmWorldPOIType.NONE)
		{
			dmCommandManager.ChatToPlayer(player, "Неизвестный тип: " + typeStr);
			return true;
		}

		// TODO: полный поиск по Points + teleport к первой точке типа.
		dmCommandManager.ChatToPlayer(player, "goto пока не реализован: тип = " + typeStr);
		return true;
	}
}
