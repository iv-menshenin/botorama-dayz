
class dmTpCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_CMD_TP;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_CMD_TP_ME)
			return HandleTeleportMe(player, parts);

		return false;
	}

	//! /tp me {X} {Z}
	private bool HandleTeleportMe(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи координаты точки: /tp me X Z");
			return false;
		}

		float x = parts[2].ToFloat();
		float z = parts[3].ToFloat();

		if ( x > 0.0 && z > 0.0 )
		{
			vector newPosition = Vector(x, 0.0, z);
			player.SetPosition(SnapToGroundExactly(newPosition));
			return true;
		}

		dmCommandManager.ChatToPlayer(player, "Что-то не так с координатами?");
		return false;
	}
}