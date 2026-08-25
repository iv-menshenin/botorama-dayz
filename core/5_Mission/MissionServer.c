//! dmBotorama — server-side chat commands (Layer 0).
//!
//! Reads the game chat. Messages starting with "/" are treated as commands.
//! "/bot spawn test" spawns a bot one meter in front of the sender and makes it
//! track the sender's face every tick.

modded class MissionServer
{
	override void OnEvent(EventType eventTypeId, Param params)
	{
		if (eventTypeId == ChatMessageEventTypeID)
		{
			ChatMessageEventParams chat = ChatMessageEventParams.Cast(params);
			if (chat)
			{
				string message = chat.param3;
				dmBotLog.Debug("OnEvent() chat: sender=" + chat.param2 + " msg='" + message + "'");

				if (message.Length() > 0 && message.Substring(0, 1) == "/")
				{
					dmBotLog.Debug("OnEvent() looks like a command, dispatching HandleChatCommand");
					if (HandleChatCommand(chat.param2, message))
						return;
				}
			}
		}

		super.OnEvent(eventTypeId, params);
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		//! Heartbeat for all bots.
		dmAISurvivor.TickAll(timeslice);
	}

	//! First word decides the command family (e.g. "bot").
	bool HandleChatCommand(string playerName, string message)
	{
		array<string> parts = new array<string>();
		message.Trim().Split(" ", parts);

		if (parts.Count() < 1)
		{
			dmBotLog.Debug("HandleChatCommand() no parts");
			return false;
		}

		string cmd = parts[0];
		if (cmd.Length() > 0 && cmd.Substring(0, 1) == "/")
			cmd = cmd.Substring(1, cmd.Length() - 1);

		dmBotLog.Debug("HandleChatCommand() cmd='" + cmd + "' parts=" + parts.Count());

		if (cmd == DM_CHAT_CMD)
			return HandleBotCommand(playerName, parts);

		return false;
	}

	//! Second word decides the sub-command (e.g. "spawn").
	bool HandleBotCommand(string playerName, array<string> parts)
	{
		if (parts.Count() < 2)
		{
			dmBotLog.Debug("HandleBotCommand() no sub-command");
			return false;
		}

		dmBotLog.Debug("HandleBotCommand() sub='" + parts[1] + "'");

		if (parts[1] == DM_CHAT_SPAWN)
			return HandleBotSpawn(playerName, parts);

		return false;
	}

	//! Third word decides the spawn kind (e.g. "test").
	bool HandleBotSpawn(string playerName, array<string> parts)
	{
		if (parts.Count() < 3)
		{
			dmBotLog.Debug("HandleBotSpawn() no spawn kind");
			return false;
		}

		dmBotLog.Debug("HandleBotSpawn() kind='" + parts[2] + "'");

		if (parts[2] == DM_CHAT_TEST)
			return HandleBotSpawnTest(playerName);

		return false;
	}

	//! Spawn a bot one meter in front of the player, tracking their face.
	bool HandleBotSpawnTest(string playerName)
	{
		dmBotLog.Debug("HandleBotSpawnTest() playerName=" + playerName);

		PlayerBase player = FindPlayerByName(playerName);
		if (!player)
		{
			dmBotLog.Debug("HandleBotSpawnTest() player not found");
			return false;
		}

		//! One meter in front of the player.
		vector spawnPos = player.GetPosition() + player.GetDirection() * DM_SPAWN_DISTANCE;
		dmBotLog.Debug("HandleBotSpawnTest() playerPos=" + player.GetPosition() + " dir=" + player.GetDirection() + " spawnPos=" + spawnPos);

		ref dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(spawnPos, Vector(0, 0, 0));

		if (pawn)
		{
			//! Face the player, then keep watching their face every tick.
			bot.SetDirection(player.GetPosition() - spawnPos);
			bot.SetLookTarget(player);

			GetGame().ChatMP(player, "Бот заспавнен (test). Всего ботов: " + dmAISurvivor.Count(), "colorAction");
		}
		else
		{
			GetGame().ChatMP(player, "Не удалось заспавнить бота.", "colorAction");
		}

		return true;
	}

	PlayerBase FindPlayerByName(string name)
	{
		name.ToLower();
		dmBotLog.Trace("FindPlayerByName() name=" + name);

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		dmBotLog.Trace("FindPlayerByName() players=" + players.Count());

		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity())
			{
				string playerName = player.GetIdentity().GetName();
				playerName.ToLower();
				if (playerName == name)
				{
					dmBotLog.Debug("FindPlayerByName() found " + name);
					return player;
				}
			}
		}

		dmBotLog.Debug("FindPlayerByName() not found: " + name);
		return null;
	}
}
