//! dmIllumination — perception (lighting): continuous daylight factor and the
//! light-scaled visibility distance per target type.
//!
//! Light is currently GLOBAL (time-of-day only): DaylightFactor() is a pure
//! function of the world clock (GetDate + the vanilla sunrise/sunset tables) and
//! GetMaxDistFor() maps it linearly onto the per-type day distance. Position-dependent
//! darkening (underground bunkers, A2) and local light sources (A4–A6) are later
//! atoms; the brain (dmAISurvivor) caches the factor every
//! DM_ILLUMINATION_CACHE_INTERVAL seconds so per-tick consumers (dmVision) don't
//! recompute the clock. The moon is NOT used here (its semantics are unconfirmed —
//! see docs/research/lighting.md): the night base is the fixed DM_VIS_DIST_NIGHT.

//! Тип цели для дальности видимости (per-type дневной радиус).
enum dmIllumTargetType
{
	PLAYER,
	ANIMAL,
	ZOMBIE
};

class dmIllumination
{
	//! Непрерывный фактор дневного света 0..1: 1 = день, 0 = ночь, линейный рамп
	//! на рассвете/закате (окно DM_DAWN_DUSK_WINDOW минут вокруг восхода/заката).
	float DaylightFactor()
	{
		int year;
		int month;
		int day;
		int hour;
		int minute;
		g_Game.GetWorld().GetDate(year, month, day, hour, minute);

		float monthday = (float)month + (float)day / 31.0;
		WorldData worldData = g_Game.GetMission().GetWorldData();
		float sunriseMinutes = worldData.GetApproxSunriseTime(monthday) * 60.0;
		float sunsetMinutes = worldData.GetApproxSunsetTime(monthday) * 60.0;
		float nowMinutes = (float)hour * 60.0 + (float)minute;

		return Ramp(nowMinutes, sunriseMinutes, sunsetMinutes);
	}

	//! Дальность видимости из УЖЕ посчитанного фактора (кэш мозга): та же линейная
	//! формула без повторного чтения часов. Потребляется кэширующим владельцем.
	float GetMaxDistFor(int targetType, float daylightFactor)
	{
		float dayDist = DM_VIS_DIST_DAY;
		if (targetType == dmIllumTargetType.ANIMAL)
			dayDist = DM_VIS_DIST_ANIMAL;
		else if (targetType == dmIllumTargetType.ZOMBIE)
			dayDist = DM_VIS_DIST_ZOMBIE;
		return Math.Lerp(DM_VIS_DIST_NIGHT, dayDist, daylightFactor);
	}

	//! Кусочно-линейный фактор суток (минуты): ночь 0, день 1, рамп 0→1 на
	//! [восход, восход+окно] и 1→0 на [закат-окно, закат].
	private float Ramp(float nowMinutes, float sunriseMinutes, float sunsetMinutes)
	{
		if (nowMinutes < sunriseMinutes)
			return 0.0;
		if (nowMinutes < sunriseMinutes + DM_DAWN_DUSK_WINDOW)
			return (nowMinutes - sunriseMinutes) / DM_DAWN_DUSK_WINDOW;
		if (nowMinutes < sunsetMinutes - DM_DAWN_DUSK_WINDOW)
			return 1.0;
		if (nowMinutes < sunsetMinutes)
			return (sunsetMinutes - nowMinutes) / DM_DAWN_DUSK_WINDOW;
		return 0.0;
	}
}
