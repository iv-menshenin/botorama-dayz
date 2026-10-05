//! dmCOTCommandModule — интеграция команд чата через Community Online Tools (COT).
//!
//! Когда COT загружен, он перехватывает на клиенте любое сообщение, начинающееся
//! с "/", и шлёт его на сервер в JMCommandModule (вместо глобального чата).
//! Диспетчер COT плоский: command + ровно одна подкоманда, поэтому одна
//! команда-семья botorama = один модуль COT. Каждый модуль перечисляет подкоманды
//! своей семьи (диспетч по parts[1] из соответствующего dm*Command.Handle) и
//! делегирует реконструированную строку "/семья подкоманда args…" обратно в
//! dmCommandManager — сквозной роутинг без дублирования логики Handle.
//!
//! ИНВАРИАНТ: добавил подкоманду в Handle какого-то dm*Command.c — добавь её же
//! в GetSubCommandNames() соответствующего COT-модуля (иначе через COT она не
//! маршрутизируется).
//!
//! Файл целиком под #ifdef JM_COT: без COT классы JMModuleBase/JMSubCommand не
//! существуют, и modded class JMModuleConstructor дал бы "base class not found".

#ifdef JM_COT

//! Одна подкоманда COT. Переопределяет JMCommand.Execute: сохраняет гейт прав
//! (HasPermission) и делает generic-роутинг — реконструирует "/семья подкоманда
//! args…" и отдаёт в dmCommandManager.Execute(sender.GetName(), fullLine).
//! Семья хранится в m_Family (проставляется владельцем при создании).
class dmCOTCommand : JMSubCommand
{
	private string m_Family;

	void SetFamily(string family)
	{
		m_Family = family;
	}

	override void Execute(PlayerIdentity sender, array<string> arguments)
	{
		JMPlayerInstance instance;
		if (!GetPermissionsManager().HasPermission(m_Permission, sender, instance))
			return;

		string fullLine = DM_COMMAND_PREFIX + m_Family + " " + m_Command;
		int i;
		for (i = 0; i < arguments.Count(); i++)
			fullLine = fullLine + " " + arguments[i];

		dmCommandManager.GetInstance().Execute(sender.GetName(), fullLine);
	}
}

//! Базовый класс модуля одной команды-семьи ("bot", "fsm", ...). Подкласс
//! переопределяет GetFamily() и GetSubCommandNames(). Один модуль на семью
//! обязателен: JMCommandConstructor.Generate шарИт один map<string,JMCommand>
//! подкоманд между ВСЕМИ GetCommandNames() одного модуля.
class dmCOTCommandModuleBase : JMModuleBase
{
	void dmCOTCommandModuleBase()
	{
		GetPermissionsManager().RegisterPermission(DM_COT_PERMISSION);
	}

	override void EnableUpdate()
	{
	}

	override array<string> GetCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(GetFamily());
		return names;
	}

	override void GetSubCommands(inout array<ref JMCommand> commands)
	{
		array<string> names = GetSubCommandNames();
		string family = GetFamily();
		int i;
		for (i = 0; i < names.Count(); i++)
		{
			//! Третий аргумент (m_Function) не используется — Execute переопределён.
			dmCOTCommand cmd = new dmCOTCommand(this, names[i], "", DM_COT_PERMISSION);
			cmd.SetFamily(family);
			commands.Insert(cmd);
		}
	}

	string GetFamily()
	{
		return "";
	}

	array<string> GetSubCommandNames()
	{
		return new array<string>();
	}
}

//! "/bot ..." — 26 глаголов (dmBotCommand.Handle, диспетч по parts[1]).
class dmCOTBotModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_CMD;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_SPAWN);
		names.Insert(DM_CHAT_INTENT);
		names.Insert(DM_CHAT_PATROL);
		names.Insert(DM_CHAT_SPEED);
		names.Insert(DM_CHAT_STATUS);
		names.Insert(DM_CHAT_DEADMANS);
		names.Insert(DM_CHAT_LOADOUT);
		names.Insert(DM_CHAT_FOLLOW);
		names.Insert(DM_CHAT_VISION);
		names.Insert(DM_CHAT_GIVE);
		names.Insert(DM_CHAT_MELEE);
		names.Insert(DM_CHAT_SAY);
		names.Insert(DM_CHAT_SAYRANDOM);
		names.Insert(DM_CHAT_COMBAT);
		names.Insert(DM_CHAT_CAR);
		names.Insert(DM_CHAT_DRIVE);
		names.Insert(DM_CHAT_KILLALL);
		names.Insert(DM_CHAT_CLEARALL);
		names.Insert(DM_CHAT_SETHEALTH);
		names.Insert(DM_CHAT_SETBLOOD);
		names.Insert(DM_CHAT_SETSHOCK);
		names.Insert(DM_CHAT_SETSTAMINA);
		names.Insert(DM_CHAT_SETHEATBUFFER);
		names.Insert(DM_CHAT_SETTOXICITY);
		names.Insert(DM_CHAT_SETENERGY);
		names.Insert(DM_CHAT_SETWATER);
		return names;
	}
}

//! "/fsm ..." — new/add/apply (dmFSMCommand.Handle).
class dmCOTFSMModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_FSM;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_FSM_NEW);
		names.Insert(DM_CHAT_ADD);
		names.Insert(DM_CHAT_FSM_APPLY);
		return names;
	}
}

//! "/test ..." — cancel/bot (dmTestCommand.Handle: parts[1] == cancel | bot).
class dmCOTTestModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_CMD_TEST;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_TEST_CANCEL);
		names.Insert(DM_CHAT_CMD);
		return names;
	}
}

//! "/prof ..." — dump/clear/start/stop (dmProfCommand.Handle).
class dmCOTProfModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_PROF;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_PROF_DUMP);
		names.Insert(DM_CHAT_PROF_CLEAR);
		names.Insert(DM_CHAT_PROF_START);
		names.Insert(DM_CHAT_PROF_STOP);
		return names;
	}
}

//! "/tp ..." — me (dmTpCommand.Handle).
class dmCOTTpModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_CMD_TP;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_CMD_TP_ME);
		return names;
	}
}

//! "/w ..." — version/position/agression/rc (dmShowCommand.Handle).
class dmCOTShowModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_SHOW;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_SHOW_VERSION);
		names.Insert(DM_CHAT_SHOW_POSITION);
		names.Insert(DM_CHAT_SHOW_AGRESSION);
		names.Insert(DM_CHAT_SHOW_RAYCAST);
		return names;
	}
}

//! "/launch ..." — invasion (dmLaunchCommand.Handle).
class dmCOTLaunchModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_LAUNCH;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_LAUNCH_INVASION);
		return names;
	}
}

//! "/poi ..." — goto (dmPoiCommand.Handle: parts[1] == "goto").
class dmCOTPoiModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_POI;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_POI_GOTO);
		return names;
	}
}

//! "/spawncity" — голый императив без подкоманды (dmSpawnCityCommand.Handle не
//! читает parts). COT требует подкоманду, поэтому регистрируем одну "run";
//! dmSpawnCityCommand.Handle игнорирует её и всё равно вызывает SpawnAll().
class dmCOTSpawnCityModule : dmCOTCommandModuleBase
{
	override string GetFamily()
	{
		return DM_CHAT_SPAWNCITY;
	}

	override array<string> GetSubCommandNames()
	{
		array<string> names = new array<string>();
		names.Insert(DM_CHAT_SPAWNCITY_RUN);
		return names;
	}
}

#endif
