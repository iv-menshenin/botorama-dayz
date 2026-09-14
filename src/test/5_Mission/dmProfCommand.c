//! dmProfCommand — команды "/prof ...": управление профайлером dmBotProfiler.
//!
//!   /prof dump  — записать накопленные замеры в CSV-файл в папку
//!                 $profile:dmBotorama/profile/ (имя profile_HH-MM-SS.csv);
//!                 путь файла приходит в чат.
//!   /prof clear — сбросить накопленные замеры, не выключая профилирование.
//!   /prof start — включить накопление замеров (по умолчанию включено с загрузки).
//!   /prof stop  — выключить накопление до следующего "/prof start".
//!
//! Типичный цикл замера сценария:
//!   /prof stop → /prof clear → /prof start → <сценарий> → /prof stop → /prof dump

class dmProfCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_PROF;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_PROF_DUMP)
			return HandleDump(player);
		if (parts[1] == DM_CHAT_PROF_CLEAR)
			return HandleClear(player);
		if (parts[1] == DM_CHAT_PROF_START)
			return HandleStart(player);
		if (parts[1] == DM_CHAT_PROF_STOP)
			return HandleStop(player);

		return false;
	}

	private bool HandleDump(PlayerBase player)
	{
		string path = dmBotProfiler.Dump();
		if (path == "")
		{
			dmCommandManager.ChatToPlayer(player, "Профайлер: нечего дампить (нет накопленных спанов)");
			return true;
		}

		dmCommandManager.ChatToPlayer(player, "Профайлер: дамп сохранён в " + path);
		return true;
	}

	private bool HandleClear(PlayerBase player)
	{
		dmBotProfiler.Clear();
		dmCommandManager.ChatToPlayer(player, "Профайлер: накопление очищено");
		return true;
	}

	private bool HandleStart(PlayerBase player)
	{
		dmBotProfiler.SetEnabled(true);
		dmCommandManager.ChatToPlayer(player, "Профайлер: профилирование включено");
		return true;
	}

	private bool HandleStop(PlayerBase player)
	{
		dmBotProfiler.SetEnabled(false);
		dmCommandManager.ChatToPlayer(player, "Профайлер: профилирование остановлено");
		return true;
	}
}
