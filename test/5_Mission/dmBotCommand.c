//! dmBotCommand — команды "/bot ...": спавн бота и управление им.
//!
//!   /bot spawn {preset}        — заспавнить тестового бота в метре перед игроком
//!                                 и привязать его к игроку (все последующие
//!                                 команды действуют именно на него).
//!   /bot intent lookAt         — бот смотрит в точку, куда смотрит игрок.
//!   /bot intent lookAtMe       — бот смотрит на игрока.
//!   /bot intent goto           — бот идёт в точку, куда смотрит игрок.
//!   /bot intent stance erect|crouch|prone — держать заданную стойку.
//!   /bot intent crouch         — красться (короткий интент, ~5 минут).
//!   /bot intent clear          — сбросить все командные намерения бота.
//!   /bot patrol add            — добавить точку патруля в точку взгляда игрока.
//!   /bot patrol clear          — очистить точки патруля.
//!   /bot speed walk|jog|sprint — задать предпочтительную скорость ходьбы.
//!   /bot loadout {name}        — применить loadout к боту (или список без имени).
//!   /bot follow [stop]         — сопровождать игрока (бок о бок); "stop" — сброс.
//!   /bot vision                — показать видимые цели бота.
//!   /bot give {item}            — выдать предмет в руки бота.
//!   /bot melee                  — ударить враждебную цель (мили-удар).
//!   /bot combat                 — боевой режим (атакует угрозы в радиусе).
//!   /bot car sitdown            — сесть в машину с игроком на свободное место.
//!
//! Интенты добавляются в командный пул (приоритет CRITICAL), поэтому они
//! перебивают автоматическое поведение; "/bot intent clear" возвращает бота
//! к автомату.

class dmBotCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_CMD;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_SPAWN)
			return HandleSpawn(player, parts);
		if (parts[1] == DM_CHAT_INTENT)
			return HandleIntent(player, parts);
		if (parts[1] == DM_CHAT_PATROL)
			return HandlePatrol(player, parts);
		if (parts[1] == DM_CHAT_SPEED)
			return HandleSpeed(player, parts);
		if (parts[1] == DM_CHAT_STATUS)
			return HandleStatus(player);
		if (parts[1] == DM_CHAT_DEADMANS)
			return HandleDeadMans(player);
		if (parts[1] == DM_CHAT_LOADOUT)
			return HandleLoadout(player, parts);
		if (parts[1] == DM_CHAT_FOLLOW)
			return HandleFollow(player, parts);
		if (parts[1] == DM_CHAT_VISION)
			return HandleVision(player, parts);
		if (parts[1] == DM_CHAT_GIVE)
			return HandleGive(player, parts);
		if (parts[1] == DM_CHAT_MELEE)
			return HandleMelee(player, parts);
		if (parts[1] == DM_CHAT_COMBAT)
			return HandleCombat(player, parts);
		if (parts[1] == DM_CHAT_CAR)
			return HandleCar(player, parts);
		if (parts[1] == DM_CHAT_SETHEALTH)
			return HandleSetHealth(player, parts);
		if (parts[1] == DM_CHAT_SETBLOOD)
			return HandleSetBlood(player, parts);
		if (parts[1] == DM_CHAT_SETSHOCK)
			return HandleSetShock(player, parts);
		if (parts[1] == DM_CHAT_SETSTAMINA)
			return HandleSetStamina(player, parts);
		if (parts[1] == DM_CHAT_SETHEATBUFFER)
			return HandleSetHeatBuffer(player, parts);
		if (parts[1] == DM_CHAT_SETTOXICITY)
			return HandleSetToxicity(player, parts);
		if (parts[1] == DM_CHAT_SETENERGY)
			return HandleSetEnergy(player, parts);
		if (parts[1] == DM_CHAT_SETWATER)
			return HandleSetWater(player, parts);

		return false;
	}

	//! Third word decides the spawn kind (e.g. "test").
	private bool HandleSpawn(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
			return false;

		if (parts[2] == DM_CHAT_TEST)
			return HandleSpawnTest(player, "");

		return HandleSpawnTest(player, parts[2]);
	}

	//! Spawn a bot one meter in front of the player and bind it to them.
	private bool HandleSpawnTest(PlayerBase player, string preset)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector spawnPos = player.GetPosition() + fwd * DM_SPAWN_DISTANCE;
		spawnPos[1] = 0.1;

		ref dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(SnapToGround(spawnPos), Vector(0, 0, 0));

		if (pawn)
		{
			dmCommandContext.BindBot(player, bot);
			//! Kick: real movement wakes the bot's physics so a bot spawned in the air falls.
			dmCommandContext.GiveMoveKick(bot, player);
			dmCommandManager.ChatToPlayer(player, "Бот заспавнен (test). Всего ботов: " + dmAISurvivor.Count());
		}
		else
		{
			dmCommandManager.ChatToPlayer(player, "Не удалось заспавнить бота.");
		}

		switch (preset) {
		case "survivor":
			dmLoadoutConfig cfg = dmLoadoutApplier.Load("PlayerSurvivorLoadout");
			if (cfg)
			{
				pawn = bot.GetPawn();
				if (pawn) dmLoadoutApplier.Apply(pawn, cfg);
			}
			bot.SetFSM(dmBotPreset_Survivor.Create(bot));
			break;
		case "escort":
			bot.SetFSM(dmBotPreset_Escort.Create(bot));
			break;
		case "hunter":
			bot.SetFSM(dmBotPreset_Hunter.Create(bot));
			break;
		}

		return true;
	}

	//! "intent" — third word decides the intent action.
	private bool HandleIntent(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
			return false;

		string action = parts[2];
		if (action == DM_CHAT_LOOKAT)
			return HandleIntentLookAt(player);
		if (action == DM_CHAT_LOOKATME)
			return HandleIntentLookAtMe(player);
		if (action == DM_CHAT_GOTO)
			return HandleIntentGoto(player);
		if (action == DM_CHAT_STANCE)
			return HandleIntentStance(player, parts);
		if (action == DM_CHAT_STANCE_CROUCH)
			return HandleIntentCrouch(player);
		if (action == DM_CHAT_CLEAR)
			return HandleIntentClear(player);

		return false;
	}

	//! Bot looks at the point the player is looking at.
	private bool HandleIntentLookAt(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		vector point;
		dmCommandManager.GetPlayerLookPoint(player, point);

		dmBotIntent_HoldLook look = new dmBotIntent_HoldLook();
		look.m_Point = point;
		look.m_Turn = dmBotLookTurn.FULL;
		look.m_Priority = dmBotIntentPriority.CRITICAL;
		look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		look.m_Deadline = DM_TEST_LOOK_DEADLINE;
		bot.AddCommandIntent(look);

		dmCommandManager.ChatToPlayer(player, "Смотрю в точку " + point);
		return true;
	}

	//! Bot looks at the player.
	private bool HandleIntentLookAtMe(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		dmBotIntent_HoldLook look = new dmBotIntent_HoldLook();
		look.m_Entity = player;
		look.m_Turn = dmBotLookTurn.FULL;
		look.m_Priority = dmBotIntentPriority.CRITICAL;
		look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		look.m_Deadline = DM_TEST_LOOK_DEADLINE;
		bot.AddCommandIntent(look);

		dmCommandManager.ChatToPlayer(player, "Смотрю на тебя");
		return true;
	}

	//! Bot walks to the point the player is looking at.
	private bool HandleIntentGoto(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		vector point;
		dmCommandManager.GetPlayerLookPoint(player, point);

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = point;
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		move.m_Deadline = DM_TEST_COMMAND_DEADLINE;
		bot.AddCommandIntent(move);

		dmCommandManager.ChatToPlayer(player, "Иду в точку " + point);
		return true;
	}

	//! Bot holds a stance (erect/crouch/prone).
	private bool HandleIntentStance(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи стойку: /bot intent stance erect|crouch|prone");
			return false;
		}

		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		string stanceName = parts[3];
		int stanceIdx;
		if (stanceName == DM_CHAT_STANCE_ERECT)
			stanceIdx = DayZPlayerConstants.STANCEIDX_ERECT;
		else if (stanceName == DM_CHAT_STANCE_CROUCH)
			stanceIdx = DayZPlayerConstants.STANCEIDX_CROUCH;
		else if (stanceName == DM_CHAT_STANCE_PRONE)
			stanceIdx = DayZPlayerConstants.STANCEIDX_PRONE;
		else
		{
			dmCommandManager.ChatToPlayer(player, "Неизвестная стойка: " + stanceName);
			return false;
		}

		dmBotIntent_Stance stance = new dmBotIntent_Stance();
		stance.m_Stance = stanceIdx;
		stance.m_Priority = dmBotIntentPriority.CRITICAL;
		stance.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		stance.m_Deadline = DM_TEST_COMMAND_DEADLINE;
		bot.AddCommandIntent(stance);

		dmCommandManager.ChatToPlayer(player, "Стойка: " + stanceName + " (сброс — /bot intent clear)");
		return true;
	}

	//! Shortcut: crouch for a limited time (5 minutes), then auto-stand.
	private bool HandleIntentCrouch(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		dmBotIntent_Stance stance = new dmBotIntent_Stance();
		stance.m_Stance = DayZPlayerConstants.STANCEIDX_CROUCH;
		stance.m_Priority = dmBotIntentPriority.CRITICAL;
		stance.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		stance.m_Deadline = DM_TEST_STANCE_DEADLINE;
		bot.AddCommandIntent(stance);

		dmCommandManager.ChatToPlayer(player, "Крадучись (5 минут)");
		return true;
	}

	//! Clear the player's bot command-intent pool.
	private bool HandleIntentClear(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		bot.ClearCommandIntents();

		dmCommandManager.ChatToPlayer(player, "Намерения очищены");
		return true;
	}

	//! "patrol" — third word decides the action (add/clear).
	private bool HandlePatrol(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
			return false;

		if (parts[2] == DM_CHAT_ADD)
			return HandlePatrolAdd(player);
		if (parts[2] == DM_CHAT_CLEAR)
			return HandlePatrolClear(player);

		return false;
	}

	private bool HandlePatrolAdd(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		vector point;
		dmCommandManager.GetPlayerLookPoint(player, point);
		bot.AddPatrolPoint(point);

		dmCommandManager.ChatToPlayer(player, "Точка патруля добавлена: " + point);
		return true;
	}

	private bool HandlePatrolClear(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		bot.ClearPatrolPoints();
		dmCommandManager.ChatToPlayer(player, "Точки патруля очищены");
		return true;
	}

	//! "speed" — third word is the preferred movement speed (walk/jog/sprint).
	private bool HandleSpeed(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи скорость: /bot speed walk|jog|sprint");
			return false;
		}

		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		string speedName = parts[2];
		float speed;
		if (speedName == DM_CHAT_SPEED_WALK)
			speed = 1.0;
		else if (speedName == DM_CHAT_SPEED_JOG)
			speed = 2.0;
		else if (speedName == DM_CHAT_SPEED_SPRINT)
			speed = 3.0;
		else
		{
			dmCommandManager.ChatToPlayer(player, "Неизвестная скорость: " + speedName);
			return false;
		}

		bot.SetPreferredSpeed(speed);
		dmCommandManager.ChatToPlayer(player, "Предпочтительная скорость: " + speedName);
		return true;
	}

	//! "/bot status" — full body/brain state report.
	private bool HandleStatus(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "У бота нет пешки");
			return false;
		}

		StaminaHandler sh = pawn.GetStaminaHandler();

		string state = "мёртв";
		if (pawn.IsAlive())
		{
			if (pawn.IsUnconscious())
				state = "БЕЗ СОЗНАНИЯ";
			else
				state = "в сознании";
		}
		if (pawn.IsRestrained())
			state += ", связан";

		string bleeding = "";
		if (pawn.IsBleeding())
			bleeding = ", кровоточит";

		string statusLine = "Состояние: " + state + bleeding;
		statusLine += " | Health=" + Fmt(pawn.GetHealth01());
		statusLine += " Blood=" + Fmt(pawn.GetHealth("", "Blood"));
		statusLine += " Shock=" + Fmt(pawn.GetHealth("", "Shock"));
		dmCommandManager.ChatToPlayer(player, statusLine);

		string stamina = "n/a";
		if (sh)
			stamina = Fmt(sh.GetStaminaNormalized()) + " (cap " + Fmt(sh.GetStaminaCap()) + ")";

		string statsLine = "Статы: Stamina=" + stamina;
		statsLine += " HeatBuffer=" + Fmt(pawn.GetStatHeatBuffer().Get());
		statsLine += " HeatComfort=" + Fmt(pawn.GetStatHeatComfort().Get());
		statsLine += " Tremor=" + Fmt(pawn.GetStatTremor().Get());
		statsLine += " Wet=" + pawn.GetStatWet().Get();
		statsLine += " Toxicity=" + Fmt(pawn.GetStatToxicity().Get());
		statsLine += " Energy=" + Fmt(pawn.GetStatEnergy().Get());
		statsLine += " Water=" + Fmt(pawn.GetStatWater().Get());
		dmCommandManager.ChatToPlayer(player, statsLine);

		dmBotFSM fsm = bot.GetFSM();
		string fsmName = "нет FSM";
		if (fsm && fsm.GetCurrentState())
			fsmName = fsm.GetCurrentState().GetName();
		dmCommandManager.ChatToPlayer(player, "Мозг: FSM=" + fsmName + " | FSM-интентов=" + bot.GetFSMIntents().Count());

		return true;
	}

	//! "/bot deadmans" — report the vanilla corpse-decay state (MissionServer's
	//! m_DeadPlayersArray + CorpseData) to diagnose why corpses don't rot.
	private bool HandleDeadMans(PlayerBase player)
	{
		MissionServer ms = MissionServer.Cast(g_Game.GetMission());
		if (!ms)
		{
			dmCommandManager.ChatToPlayer(player, "Нет MissionServer");
			return true;
		}

		int total = ms.m_DeadPlayersArray.Count();
		dmCommandManager.ChatToPlayer(player, "Трупов в m_DeadPlayersArray: " + total);

		float snowMin;
		float snowMax;
		g_Game.GetWeather().GetSnowfall().GetLimits(snowMin, snowMax);
		dmCommandManager.ChatToPlayer(player, "Snowfall max=" + Fmt(snowMax) + " (decayEffects=" + (snowMax <= 0) + ")");

		int i;
		for (i = 0; i < total; i++)
		{
			CorpseData cd = ms.m_DeadPlayersArray.Get(i);
			if (!cd || !cd.m_Player)
				continue;

			bool isBot = dmAISurvivorBase.Cast(cd.m_Player) != null;

			string line = "[" + i + "] " + isBot + " corpseState=" + cd.m_iCorpseState;
			line += " bUpdate=" + cd.m_bUpdate;
			line += " maxLifetime=" + cd.m_iMaxLifetime;
			line += " lifetimeAdj=" + Fmt(cd.m_LifetimeAdjusted);
			line += " lastLifetime=" + Fmt(cd.m_LastLifetime);
			line += " | pawn: lifetime=" + Fmt(cd.m_Player.GetLifetime());
			line += " frozen=" + cd.m_Player.GetIsFrozen();
			line += " temp=" + Fmt(cd.m_Player.GetTemperature());
			line += " corpseStateField=" + cd.m_Player.m_CorpseState;
			dmCommandManager.ChatToPlayer(player, line);
		}

		return true;
	}

	//! "/bot loadout {name}" — apply a loadout to the bound bot; without a name,
	//! list the loadout files available in DM_LOADOUT_DIR.
	private bool HandleLoadout(PlayerBase player, array<string> parts)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		if (parts.Count() < 3)
		{
			array<string> names = new array<string>();
			dmLoadoutApplier.List(names);
			if (names.Count() == 0)
			{
				dmCommandManager.ChatToPlayer(player, "Нет loadout-файлов в " + DM_LOADOUT_DIR);
				return true;
			}

			string line = "Loadout-ы:";
			for (int i = 0; i < names.Count(); i++)
				line += " " + names[i];
			dmCommandManager.ChatToPlayer(player, line);
			return true;
		}

		string name = parts[2];
		dmLoadoutConfig cfg = dmLoadoutApplier.Load(name);
		if (!cfg)
		{
			dmCommandManager.ChatToPlayer(player, "Loadout '" + name + "' не найден в " + DM_LOADOUT_DIR);
			return true;
		}

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "У бота нет пешки");
			return true;
		}

		dmLoadoutApplier.Apply(pawn, cfg);
		dmCommandManager.ChatToPlayer(player, "Loadout '" + name + "' применён");
		return true;
	}

	//! "/bot follow [stop]" — make the bound bot escort the player (alongside);
	//! "stop" clears the follow target and the escort FSM.
	private bool HandleFollow(PlayerBase player, array<string> parts)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		if (parts.Count() >= 3 && parts[2] == DM_CHAT_STOP)
		{
			bot.SetFollowTarget(null);
			bot.ClearFSMIntents();
			dmCommandManager.ChatToPlayer(player, "Сопровождение остановлено");
			return true;
		}

		bot.SetFollowTarget(player);
		bot.ClearFSMIntents();
		bot.SetFSM(dmBotPreset_Escort.Create(bot));
		dmCommandManager.ChatToPlayer(player, "Сопровождаю (сброс — /bot follow stop)");
		return true;
	}

	//! "/bot vision" — print the bot's visible targets.
	private bool HandleVision(PlayerBase player, array<string> parts)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		ref array<ref dmTarget> targets = bot.GetTargets();
		if (targets.Count() == 0)
		{
			dmCommandManager.ChatToPlayer(player, "Никого не вижу");
			return true;
		}

		int i;
		for (i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			string typeName = "null";
			if (t.m_Entity)
				typeName = t.m_Entity.GetType();

			int los = 0;
			if (t.m_HasLOS)
				los = 1;
			int friendly = 0;
			if (t.m_Friendly)
				friendly = 1;
			float age = GetGame().GetTickTime() - t.m_LastContact;

			string line = "[" + i + "] " + typeName;
			line += " pos=" + t.m_LastPosition;
			line += " LOS=" + los;
			line += " age=" + age;
			line += " threat=" + t.m_Threat;
			line += " attract=" + t.m_Attractiveness;
			line += " friendly=" + friendly;
			dmCommandManager.ChatToPlayer(player, line);
		}

		return true;
	}

	//! "/bot give {item}" — hand a class directly into the bound bot's hands.
	private bool HandleGive(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи класс: /bot give <item>");
			return true;
		}

		string cls = parts[2];
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "У бота нет пешки");
			return true;
		}

		EntityAI item = pawn.GetHumanInventory().CreateInHands(cls);
		if (item)
		{
			dmCommandManager.ChatToPlayer(player, "Выдал " + cls);
			return true;
		}

		dmCommandManager.ChatToPlayer(player, "Не удалось выдать " + cls);
		return true;
	}

	//! "/bot melee" — order the bound bot to strike its current hostile target.
	private bool HandleMelee(PlayerBase player, array<string> parts)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота");
			return true;
		}

		dmTarget t = bot.GetHostileTarget();
		if (!t || !t.m_Entity)
		{
			dmCommandManager.ChatToPlayer(player, "Нет враждебной цели в радиусе");
			return true;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "Нет пешки");
			return true;
		}

		pawn.RequestMeleeAttack(t.m_Entity);
		dmCommandManager.ChatToPlayer(player, "Удар по " + t.m_Entity.GetType());
		return true;
	}

	//! "/bot combat" — switch the bound bot to the combat preset (Idle + Fighting).
	private bool HandleCombat(PlayerBase player, array<string> parts)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		bot.SetFSM(dmBotPreset_Combat.Create(bot));
		dmCommandManager.ChatToPlayer(player, "Боевой режим (атакует угрозы в радиусе)");
		return true;
	}

	//! "/bot car sitdown" — find a car where a real player is sitting and make the
	//! bound bot get in on a free passenger seat. "/bot car getout" — make the
	//! bound bot exit the vehicle it is currently seated in.
	private bool HandleCar(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи: /bot car sitdown | getout");
			return true;
		}

		if (parts[2] == DM_CHAT_CAR_SITDOWN)
			return HandleCarSitdown(player);

		if (parts[2] == DM_CHAT_CAR_GETOUT)
			return HandleCarGetout(player);

		dmCommandManager.ChatToPlayer(player, "Укажи: /bot car sitdown | getout");
		return true;
	}

	dmBotIntent_GetInVehicle m_intentGetInVehicle;

	private bool HandleCarSitdown(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		int freeSeat;
		Transport transport = FindCarWithPlayer(freeSeat);
		if (!transport)
		{
			dmCommandManager.ChatToPlayer(player, "Не нашёл машину с игроком (или нет свободного места)");
			return true;
		}

		if ( !m_intentGetInVehicle )
		{
			dmBotIntent_GetInVehicle m_intentGetInVehicle = new dmBotIntent_GetInVehicle();
			m_intentGetInVehicle.m_Transport = transport;
			m_intentGetInVehicle.m_Seat = freeSeat;
			bot.AddCommandIntent(m_intentGetInVehicle);
			dmCommandManager.ChatToPlayer(player, "Сажусь в " + transport.GetType() + " место " + freeSeat);
		}

		return true;
	}

	private bool HandleCarGetout(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return true;
		}

		if ( m_intentGetInVehicle )
		{
			m_intentGetInVehicle.Finish();
			m_intentGetInVehicle = null;
			dmCommandManager.ChatToPlayer(player, "Выхожу из машины");
		}
		return true;
	}

	//! Find a vehicle with a real (non-bot) player aboard and a free seat. Returns
	//! the transport and the free seat index via out param.
	private Transport FindCarWithPlayer(out int freeSeat)
	{
		freeSeat = -1;
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p || !p.IsAlive())
				continue;
			if (dmAISurvivorBase.Cast(p) != null)
				continue; // бот — пропускаем
			Transport t = Transport.Cast(p.GetParent());
			if (!t)
				continue;
			int s;
			for (s = 0; s < t.CrewSize(); s++)
			{
				if (!t.CrewMember(s))
				{
					freeSeat = s;
					return t;
				}
			}
		}
		return null;
	}

	//! Resolve the bound bot and parse the required float argument.
	//! Returns null (and replies an error) when the command is malformed.
	private dmAISurvivor ResolveForSet(PlayerBase player, array<string> parts, out float value)
	{
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи значение: /bot " + parts[1] + " <число>");
			return null;
		}

		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return null;
		}

		value = parts[2].ToFloat();
		return bot;
	}

	private bool HandleSetHealth(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().SetHealth("", "Health", value);
		dmCommandManager.ChatToPlayer(player, "Health = " + value);
		return true;
	}

	private bool HandleSetBlood(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().SetHealth("", "Blood", value);
		dmCommandManager.ChatToPlayer(player, "Blood = " + value);
		return true;
	}

	private bool HandleSetShock(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().SetHealth("", "Shock", value);
		dmCommandManager.ChatToPlayer(player, "Shock = " + value);
		return true;
	}

	private bool HandleSetStamina(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		StaminaHandler sh = bot.GetPawn().GetStaminaHandler();
		if (sh)
			sh.SetStamina(value);
		dmCommandManager.ChatToPlayer(player, "Stamina = " + value);
		return true;
	}

	private bool HandleSetHeatBuffer(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().GetStatHeatBuffer().Set(value);
		dmCommandManager.ChatToPlayer(player, "HeatBuffer = " + value);
		return true;
	}

	private bool HandleSetToxicity(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().GetStatToxicity().Set(value);
		dmCommandManager.ChatToPlayer(player, "Toxicity = " + value);
		return true;
	}

	private bool HandleSetEnergy(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().GetStatEnergy().Set(value);
		dmCommandManager.ChatToPlayer(player, "Energy = " + value);
		return true;
	}

	private bool HandleSetWater(PlayerBase player, array<string> parts)
	{
		float value;
		dmAISurvivor bot = ResolveForSet(player, parts, value);
		if (!bot) return true;
		bot.GetPawn().GetStatWater().Set(value);
		dmCommandManager.ChatToPlayer(player, "Water = " + value);
		return true;
	}

	//! Trim a float to 3 decimals for readable chat output.
	private static string Fmt(float v)
	{
		return (Math.Round(v * 1000.0) / 1000.0).ToString();
	}
}

vector SnapToGround(vector pos)
{
	float pos_x = pos[0];
	float pos_z = pos[2];
	float pos_y = g_Game.SurfaceY(pos_x, pos_z);
	vector tmp_pos = Vector(pos_x, pos_y, pos_z);
	tmp_pos[1] = tmp_pos[1] + pos[1];

	return tmp_pos;
}