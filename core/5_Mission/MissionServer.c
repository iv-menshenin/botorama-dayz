//! dmBotorama — server-side chat commands (Layer 0).
//!
//! Reads the game chat. Messages starting with "/" are treated as commands:
//!  - "/bot spawn test"        spawn a bot and bind it to the sender.
//!  - "/bot intent lookAt"     bot looks at the point the sender is looking at.
//!  - "/bot intent lookAtMe"   bot looks at the sender.
//!  - "/bot intent goto"       bot walks to the point the sender is looking at.
//!  - "/bot intent clear"      clear the sender's bot command-intent pool.

modded class MissionServer
{
	static ref map<string, ref dmAISurvivor> s_TestBotByPlayer = new map<string, ref dmAISurvivor>;

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

	//! Second word decides the sub-command (e.g. "spawn", "intent").
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

		if (parts[1] == DM_CHAT_INTENT)
			return HandleIntentCommand(playerName, parts);

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

	//! Spawn a bot one meter in front of the player and bind it to them.
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
			string key = playerName;
			key.ToLower();
			s_TestBotByPlayer.Set(key, bot);

			GetGame().ChatMP(player, "Бот заспавнен (test). Всего ботов: " + dmAISurvivor.Count(), "colorAction");
		}
		else
		{
			GetGame().ChatMP(player, "Не удалось заспавнить бота.", "colorAction");
		}

		return true;
	}

	//! "intent" — third word decides the intent action.
	bool HandleIntentCommand(string playerName, array<string> parts)
	{
		if (parts.Count() < 3)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("HandleIntentCommand() no action");
			#endif
			return false;
		}

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleIntentCommand() action='" + parts[2] + "'");
		#endif

		string action = parts[2];
		if (action == DM_CHAT_LOOKAT)
			return HandleIntentLookAt(playerName);
		if (action == DM_CHAT_LOOKATME)
			return HandleIntentLookAtMe(playerName);
		if (action == DM_CHAT_GOTO)
			return HandleIntentGoto(playerName);
		if (action == DM_CHAT_CLEAR)
			return HandleIntentClear(playerName);
		return false;
	}

	//! Bot looks at the point the player is looking at (deadline, parallel, critical).
	bool HandleIntentLookAt(string playerName)
	{
		dmAISurvivor bot = FindBotForPlayer(playerName);
		if (!bot)
		{
			ChatToPlayer(playerName, "Нет бота — сначала /bot spawn test");
			return false;
		}

		PlayerBase player = FindPlayerByName(playerName);
		if (!player)
			return false;

		vector point;
		GetPlayerLookPoint(player, point);

		dmBotIntent_HoldLook look = new dmBotIntent_HoldLook();
		look.m_Point = point;
		look.m_Turn = dmBotLookTurn.FULL;
		look.m_Priority = dmBotIntentPriority.CRITICAL;
		look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		look.m_Deadline = DM_TEST_LOOK_DEADLINE;
		bot.AddCommandIntent(look);

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleIntentLookAt() point=" + point);
		#endif
		GetGame().ChatMP(player, "Смотрю в точку " + point, "colorAction");
		return true;
	}

	//! Bot looks at the player (deadline, parallel, critical).
	bool HandleIntentLookAtMe(string playerName)
	{
		dmAISurvivor bot = FindBotForPlayer(playerName);
		if (!bot)
		{
			ChatToPlayer(playerName, "Нет бота — сначала /bot spawn test");
			return false;
		}

		PlayerBase player = FindPlayerByName(playerName);
		if (!player)
			return false;

		dmBotIntent_HoldLook look = new dmBotIntent_HoldLook();
		look.m_Entity = player;
		look.m_Turn = dmBotLookTurn.FULL;
		look.m_Priority = dmBotIntentPriority.CRITICAL;
		look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		look.m_Deadline = DM_TEST_LOOK_DEADLINE;
		bot.AddCommandIntent(look);

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleIntentLookAtMe() player=" + player);
		#endif
		GetGame().ChatMP(player, "Смотрю на тебя", "colorAction");
		return true;
	}

	//! Bot walks to the point the player is looking at (no deadline, parallel, critical).
	bool HandleIntentGoto(string playerName)
	{
		dmAISurvivor bot = FindBotForPlayer(playerName);
		if (!bot)
		{
			ChatToPlayer(playerName, "Нет бота — сначала /bot spawn test");
			return false;
		}

		PlayerBase player = FindPlayerByName(playerName);
		if (!player)
			return false;

		vector point;
		GetPlayerLookPoint(player, point);

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Target = point;
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		bot.AddCommandIntent(move);

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleIntentGoto() target=" + point);
		#endif
		GetGame().ChatMP(player, "Иду в точку " + point, "colorAction");
		return true;
	}

	//! Clear the player's bot command-intent pool.
	bool HandleIntentClear(string playerName)
	{
		dmAISurvivor bot = FindBotForPlayer(playerName);
		if (!bot)
		{
			ChatToPlayer(playerName, "Нет бота — сначала /bot spawn test");
			return false;
		}

		bot.ClearCommandIntents();

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("HandleIntentClear()");
		#endif
		ChatToPlayer(playerName, "Намерения очищены");
		return true;
	}

	//! The bot bound to the player via "/bot spawn test" (or null).
	dmAISurvivor FindBotForPlayer(string playerName)
	{
		string key = playerName;
		key.ToLower();
		dmAISurvivor bot;
		if (s_TestBotByPlayer.Find(key, bot))
			return bot;
		return null;
	}

	//! World point the player is looking at (raycast from their eyes).
	void GetPlayerLookPoint(PlayerBase player, out vector point)
	{
		vector beg = player.GetPosition() + Vector(0, DM_EYE_HEIGHT, 0);
		vector dir = MiscGameplayFunctions.GetHeadingVector(player);
		vector end = beg + dir * DM_LOOK_RAYCAST_DISTANCE;
		vector contactPos;
		vector contactDir;
		int contactComponent;
		if (DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, null, null, player, false, false, ObjIntersectView))
			point = contactPos;
		else
			point = end;
	}

	void ChatToPlayer(string playerName, string msg)
	{
		PlayerBase player = FindPlayerByName(playerName);
		if (player)
			GetGame().ChatMP(player, msg, "colorAction");
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
