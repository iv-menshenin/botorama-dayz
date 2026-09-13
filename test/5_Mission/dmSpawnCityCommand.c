//! dmSpawnCityCommand — команда "/spawncity": принудительно спавнит всех ботов спавн-менеджера.
class dmSpawnCityCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_SPAWNCITY;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		dmBotSpawnManager.Get().SpawnAll();
		dmCommandManager.ChatToPlayer(player, "Спавн запущен. Всего ботов: " + dmAISurvivor.Count());
		return true;
	}
}
