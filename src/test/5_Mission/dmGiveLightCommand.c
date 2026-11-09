//! dmGiveLightCommand — команда "/givelight <предмет>": дать источнику света боту.
//!
//! Без имени бота предмет выдаётся привязанному к игроку боту (как "/bot give");
//! с именем — боту по имени из глобального реестра dmAISurvivor.s_All.
//! Головные источники (Headtorch_*/NVGoggles) кладутся в слот головы, остальные — в руки.
class dmGiveLightCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_GIVELIGHT;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи предмет: /givelight Flashlight");
			return false;
		}

		dmAISurvivor bot;
		string cls;
		if (parts.Count() >= 3)
		{
			bot = FindBotByName(parts[1]);
			cls = parts[2];
		}
		else
		{
			bot = dmCommandContext.FindBotForPlayer(player);
			cls = parts[1];
		}

		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Бот не найден.");
			return false;
		}

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "Нет пешки бота.");
			return false;
		}

		dmGiveLightTo(pawn, cls);

		dmCommandManager.ChatToPlayer(player, "Выдан свет: " + cls);
		return true;
	}

	//! Головные источники — в слот головы, остальные — в руки.
	private void dmGiveLightTo(PlayerBase pawn, string cls)
	{
		if (cls == "Headtorch_Black" || cls == "NVGoggles")
		{
			int slotId = InventorySlots.GetSlotIdFromString("Headgear");
			pawn.GetInventory().CreateAttachmentEx(cls, slotId);
		}
		else
		{
			pawn.GetHumanInventory().CreateInHands(cls);
		}
	}

	private dmAISurvivor FindBotByName(string name)
	{
		int i;
		for (i = 0; i < dmAISurvivor.s_All.Count(); i++)
		{
			dmAISurvivor b = dmAISurvivor.s_All[i];
			if (b.GetName() == name)
				return b;
		}
		return null;
	}
}
