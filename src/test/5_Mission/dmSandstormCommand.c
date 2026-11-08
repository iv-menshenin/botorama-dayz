//! dmSandstormCommand — команда "/sandstorm start|stop" для E2E-теста укрытия.
//!
//!   /sandstorm start — форсировать песчаную бурю: overcast, ветер (≥18 м/с),
//!                      ветер выравнивается под направление фронта, затем
//!                      Start(fadeInTime, false) (эталон — ванильный DEV_SET_SANDSTORM).
//!   /sandstorm stop  — Stop(0, false) — мгновенное затухание.
class dmSandstormCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_SANDSTORM;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_SANDSTORM_START)
			return HandleStart(player);
		if (parts[1] == DM_CHAT_SANDSTORM_STOP)
			return HandleStop(player);

		return false;
	}

	private bool HandleStart(PlayerBase player)
	{
		#ifdef SERVER
		Weather w = g_Game.GetWeather();

		w.GetOvercast().SetLimits(DM_SANDSTORM_OVERCAST, DM_SANDSTORM_OVERCAST);
		w.GetOvercast().Set(DM_SANDSTORM_OVERCAST, 0, 3600);
		w.GetWindMagnitude().SetLimits(DM_SANDSTORM_WIND, DM_SANDSTORM_WIND);
		w.GetWindMagnitude().Set(DM_SANDSTORM_WIND, 0, 3600);

		w.GetSandstorm().Start(DM_SANDSTORM_FADE_IN, false);

		//! Выровнять ветер под направление фронта (направление известно после Start).
		vector stormDir = w.GetSandstorm().GetDirection();
		float windAngle = Weather.WindDirectionToAngle(stormDir);
		w.GetWindDirection().Set(windAngle, 0, 3600);
		#endif

		dmCommandManager.ChatToPlayer(player, "Песчаная буря: старт (fadeIn=" + DM_SANDSTORM_FADE_IN + "с)");
		return true;
	}

	private bool HandleStop(PlayerBase player)
	{
		#ifdef SERVER
		Weather w = g_Game.GetWeather();
		if (w.GetSandstorm().IsActive())
			w.GetSandstorm().Stop(0, false);
		#endif

		dmCommandManager.ChatToPlayer(player, "Песчаная буря: стоп");
		return true;
	}
}
