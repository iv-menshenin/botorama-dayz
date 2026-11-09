//! dmSetTimeCommand — команда "/settime чч:мм": ручная установка времени суток.
//!
//! Год/месяц/день фиксированы (DM_E2E_SETTIME_*), меняются только час/минута —
//! чтобы тестировщик мог вручную прогонять день/ночь/рассвет/закат.
class dmSetTimeCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_SETTIME;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи время: /settime чч:мм");
			return false;
		}

		return HandleSetTime(player, parts[1]);
	}

	private bool HandleSetTime(PlayerBase player, string time)
	{
		array<string> parts = new array<string>();
		time.Split(":", parts);
		if (parts.Count() < 2)
		{
			dmCommandManager.ChatToPlayer(player, "Формат: /settime чч:мм");
			return false;
		}

		int hour = parts[0].ToInt();
		int minute = parts[1].ToInt();
		if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
		{
			dmCommandManager.ChatToPlayer(player, "Время вне диапазона (ч 0-23, м 0-59).");
			return false;
		}

		#ifdef SERVER
		g_Game.GetWorld().SetDate(DM_E2E_SETTIME_YEAR, DM_E2E_SETTIME_MONTH, DM_E2E_SETTIME_DAY, hour, minute);
		#endif

		dmCommandManager.ChatToPlayer(player, "Время установлено: " + time);
		return true;
	}
}
