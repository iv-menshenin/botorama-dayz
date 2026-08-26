//! dmBotorama — server-side chat commands (Layer 0).
//!
//! Reads the game chat. Messages starting with "/" are treated as commands.
//! "/bot spawn test" spawns a bot one meter in front of the sender and makes it
//! track the sender's face every tick.

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		dmBotLog.LogVersion();
	}

	override void OnEvent(EventType eventTypeId, Param params)
	{
		if (eventTypeId == ChatMessageEventTypeID)
		{
			ChatMessageEventParams chat = ChatMessageEventParams.Cast(params);
			if (chat)
			{
				string message = chat.param3;
				#ifdef DM_BOT_DEBUG
				dmBotLog.Debug("OnEvent() chat: sender=" + chat.param2 + " msg='" + message + "'");
				#endif

				if (message.Length() > 0 && message.Substring(0, 1) == "/")
				{
					#ifdef DM_BOT_DEBUG
					dmBotLog.Debug("OnEvent() looks like a command, dispatching HandleChatCommand");
					#endif
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
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("HandleChatCommand() no parts");
			#endif
			return false;
		}

		string cmd = parts[0];
		if (cmd.Length() > 0 && cmd.Substring(0, 1) == "/")
			cmd = cmd.Substring(1, cmd.Length() - 1);

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleChatCommand() cmd='" + cmd + "' parts=" + parts.Count());
		#endif

		if (cmd == DM_CHAT_CMD)
			return HandleBotCommand(playerName, parts);

		return false;
	}

	//! Second word decides the sub-command (e.g. "spawn").
	bool HandleBotCommand(string playerName, array<string> parts)
	{
		if (parts.Count() < 2)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("HandleBotCommand() no sub-command");
			#endif
			return false;
		}

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleBotCommand() sub='" + parts[1] + "'");
		#endif

		if (parts[1] == DM_CHAT_SPAWN)
			return HandleBotSpawn(playerName, parts);

		return false;
	}

	//! Third word decides the spawn kind (e.g. "test").
	bool HandleBotSpawn(string playerName, array<string> parts)
	{
		if (parts.Count() < 3)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("HandleBotSpawn() no spawn kind");
			#endif
			return false;
		}

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleBotSpawn() kind='" + parts[2] + "'");
		#endif

		if (parts[2] == DM_CHAT_TEST)
			return HandleBotSpawnTest(playerName);

		return false;
	}

	//! Spawn a bot one meter in front of the player, tracking their face.
	bool HandleBotSpawnTest(string playerName)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleBotSpawnTest() playerName=" + playerName);
		#endif

		PlayerBase player = FindPlayerByName(playerName);
		if (!player)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("HandleBotSpawnTest() player not found");
			#endif
			return false;
		}

		//! One meter in front of the player.
		vector spawnPos = player.GetPosition() + player.GetDirection() * DM_SPAWN_DISTANCE;
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleBotSpawnTest() playerPos=" + player.GetPosition() + " dir=" + player.GetDirection() + " spawnPos=" + spawnPos);
		#endif

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
		#ifdef DM_BOT_TRACE
		dmBotLog.Trace("FindPlayerByName() name=" + name);
		#endif

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		#ifdef DM_BOT_TRACE
		dmBotLog.Trace("FindPlayerByName() players=" + players.Count());
		#endif

		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity())
			{
				string playerName = player.GetIdentity().GetName();
				playerName.ToLower();
				if (playerName == name)
				{
					#ifdef DM_BOT_DEBUG
					dmBotLog.Debug("FindPlayerByName() found " + name);
					#endif
					return player;
				}
			}
		}

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("FindPlayerByName() not found: " + name);
		#endif
		return null;
	}
}
