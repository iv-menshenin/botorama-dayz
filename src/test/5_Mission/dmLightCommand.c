//! dmLightCommand — команда "/light": дамп нативов ночи (IsNight/moon/lat/lon/date).
//!
//! Значения пишутся в RPT под DM_PERCEPTION_DEBUG (для грепа) и дублируются в чат
//! игроку для ручной проверки семантики луны (методы нигде не задокументированы).
class dmLightCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_LIGHT;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		int year;
		int month;
		int day;
		int hour;
		int minute;
		g_Game.GetWorld().GetDate(year, month, day, hour, minute);

		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Light] IsNight=" + g_Game.GetWorld().IsNight());
		dmBotLog.Debug("", "[Light] moonIntensity=" + g_Game.GetWorld().GetMoonIntensity());
		dmBotLog.Debug("", "[Light] sunOrMoon=" + g_Game.GetWorld().GetSunOrMoon());
		dmBotLog.Debug("", "[Light] latitude=" + g_Game.GetWorld().GetLatitude());
		dmBotLog.Debug("", "[Light] longitude=" + g_Game.GetWorld().GetLongitude());
		dmBotLog.Debug("", "[Light] date=" + year + "-" + month + "-" + day + " " + hour + ":" + minute);
		#endif

		string line = "Night=" + g_Game.GetWorld().IsNight() + " moon=" + g_Game.GetWorld().GetMoonIntensity();
		line = line + " sunOrMoon=" + g_Game.GetWorld().GetSunOrMoon();
		line = line + " date=" + hour + ":" + minute;
		dmCommandManager.ChatToPlayer(player, line);

		return true;
	}
}
