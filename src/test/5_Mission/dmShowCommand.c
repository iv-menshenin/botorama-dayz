
class dmShowCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_SHOW;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_SHOW_VERSION)
			return HandleShowVersion(player);

		if (parts[1] == DM_CHAT_SHOW_POSITION)
			return HandleShowPosition(player);

		if (parts[1] == DM_CHAT_SHOW_AGRESSION)
			return HandleShowAgression(player);

		return false;
	}

	private bool HandleShowVersion(PlayerBase player)
	{
		dmCommandManager.ChatToPlayer(player, "Botorama version: " + DM_BOTORAMA_VERSION + " [by devalio] ©2026");
		return true;
	}

	private bool HandleShowPosition(PlayerBase player)
	{
		dmCommandManager.ChatToPlayer(player, "Player position: " + player.GetPosition());
		return true;
	}

	private bool HandleShowAgression(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
        {
		    dmCommandManager.ChatToPlayer(player, "У тебя нет своего бота");
            return false;
        }

        dmTarget t = bot.FindTarget(player);
        if ( !t )
        {
		    dmCommandManager.ChatToPlayer(player, "Бот не расценивает тебя как цель");
            return false;
        }
		dmCommandManager.ChatToPlayer(player, "Параметры цели: видит=" + t.m_HasLOS + " контакт=" + t.m_LastContact + " опасность=" + t.m_Threat);
		return true;
	}
}