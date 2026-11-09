//! dmIllumination — perception (lighting): continuous daylight factor, the
//! light-scaled visibility distance per target type, underground-bunker
//! detection (A2), and sandstorm dimming (A3).
//!
//! DaylightFactor() is a pure function of the world clock (GetDate + the vanilla
//! sunrise/sunset tables); GetMaxDistFor() maps it linearly onto the per-type day
//! distance, with hard overrides while the bot is inside an underground bunker
//! (INNER trigger, DM_VIS_DIST_BUNKER) or an active sandstorm (DM_VIS_DIST_SANDSTORM).
//! IsInBunker() is a server-side point-in-OBB over UndergroundAreaLoader.m_JsonData
//! (the vanilla trigger presence is client-only and not replicated, so the bot
//! tests itself). InSandstorm() delegates to dmSandstorm.IsPointInStorm (positional
//! GetIntensity, not the shelter-aware GetIntensityForPlayer). The brain
//! (dmAISurvivor) caches a dmLightState snapshot every
//! DM_ILLUMINATION_CACHE_INTERVAL seconds so per-tick consumers don't recompute
//! the clock or re-test the boxes. The moon (GetMoonIntensity, 0..1 phase signal —
//! 0 by day, ~0.67 by near-full-moon night) scales the night base linearly between
//! DM_VIS_DIST_NIGHT and DM_VIS_DIST_MOON. Local light sources (A4–A6) are later atoms.

//! Тип цели для дальности видимости (per-type дневной радиус).
enum dmIllumTargetType
{
	PLAYER,
	ANIMAL,
	ZOMBIE
};

//! Кэшированный снимок световых факторов (носитель для GetMaxDistFor): дневной
//! фактор, фаза луны и позиционные флаги бункера/бури. Владелец (мозг) держит
//! единственный экземпляр и заполняет его раз в DM_ILLUMINATION_CACHE_INTERVAL.
class dmLightState
{
	float daylightFactor;
	float moonIntensity;
	bool inBunker;
	bool inSandstorm;
}

//! Кэшированный INNER-триггер бункера: центр (Position), половинные размеры
//! (extents = Size*0.5) и три строки обратной матрицы поворота (Orientation,
//! Эйлер в градусах). Строится один раз из UndergroundAreaLoader.m_JsonData —
//! матрица дорогая (YawPitchRollMatrix + транспозиция), а данные триггеров
//! неизменны после MissionServer.OnInit.
class dmBunkerBox
{
	vector m_Center;
	vector m_HalfSize;
	vector m_InvR0;
	vector m_InvR1;
	vector m_InvR2;
}

class dmIllumination
{
	//! Статический кэш INNER-триггеров бункеров. Заполняется лениво из
	//! UndergroundAreaLoader.m_JsonData. Инвариант: первый вызов обязан быть после
	//! SpawnAllTriggerCarriers (missionserver) — иначе данных ещё нет и кэш
	//! остаётся null (см. BuildBunkerCache), чтобы следующий вызов пере-попробовал.
	static ref array<ref dmBunkerBox> s_BunkerCache;

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

	//! Интенсивность луны 0..1 (сигнал фазы): 0 = новолуние/день, 1 = полнолуние.
	//! Ночью ~0.67 при почти полной луне (реальная фаза, [0,1]). Расширяет ночную
	//! базу дальности видимости (DM_VIS_DIST_NIGHT → DM_VIS_DIST_MOON).
	float MoonIntensity()
	{
		return g_Game.GetWorld().GetMoonIntensity();
	}

	//! Дальность видимости из УЖЕ посчитанных факторов (кэш мозга): ночная база
	//! интерполируется по фазе луны, итог — по дневному фактору. Жёсткие override'ы:
	//! бункер (10 м) приоритетнее бури (20 м) — внутри бункера всегда темнее, чем
	//! сквозь песчаную стену, а позиционная интенсивность бури в герметичном бункере
	//! и так не имеет смысла; поэтому сначала бункер, затем буря, затем линейная
	//! интерполяция день/ночь. Потребляется кэширующим владельцем.
	float GetMaxDistFor(dmIllumTargetType targetType, dmLightState state)
	{
		if (state.inBunker)
			return DM_VIS_DIST_BUNKER;
		if (state.inSandstorm)
			return DM_VIS_DIST_SANDSTORM;
		float dayDist = DM_VIS_DIST_DAY;
		if (targetType == dmIllumTargetType.ANIMAL)
			dayDist = DM_VIS_DIST_ANIMAL;
		else if (targetType == dmIllumTargetType.ZOMBIE)
			dayDist = DM_VIS_DIST_ZOMBIE;
		float nightDist = Math.Lerp(DM_VIS_DIST_NIGHT, DM_VIS_DIST_MOON, state.moonIntensity);
		return Math.Lerp(nightDist, dayDist, state.daylightFactor);
	}

	//! Точка внутри хотя бы одного INNER-триггера бункера (EyeAccommodation < 1)?
	//! Point-in-OBB: переводим pos в локальное пространство триггера (вычесть центр,
	//! inverse-rotate по Orientation) и делаем AABB-тест |local| <= extents. Кэш
	//! триггеров/матриц — статический (не пересчитывается потиково). Нет данных
	//! бункеров (null/пустой m_JsonData) → false.
	bool IsInBunker(vector pos)
	{
		BuildBunkerCache();
		if (!s_BunkerCache)
			return false;

		int i;
		for (i = 0; i < s_BunkerCache.Count(); i++)
		{
			if (PointInBox(s_BunkerCache[i], pos))
				return true;
		}
		return false;
	}

	//! Бот внутри активной песчаной бури (позиционная интенсивность > 0)? Делегируем
	//! dmSandstorm.IsPointInStorm — единая точка «в буре ли точка» (порог 0 = любая
	//! ненулевая интенсивность). Не GetIntensityForPlayer — чтобы не задваивать учёт
	//! укрытий (крыша/подземелье/машина), который уже закрыт бункером (см.
	//! GetMaxDistFor) и будущими атомами укрытий.
	bool InSandstorm(vector pos)
	{
		return dmSandstorm.IsPointInStorm(pos, DM_SANDSTORM_VIS_THRESHOLD);
	}

	//! Ленивое заполнение статического кэша INNER-триггеров. Повторный вызов — no-op
	//! (m_JsonData неизменен после MissionServer.OnInit). Если данных ещё нет
	//! (!data || !data.Triggers) — кэш остаётся null, чтобы следующий вызов
	//! пере-попробовал (первый вызов обязан быть после SpawnAllTriggerCarriers).
	//! OUTER-триггеры (EyeAccommodation >= 1.0) пропускаются — в них не темно.
	private void BuildBunkerCache()
	{
		if (s_BunkerCache)
			return;

		JsonUndergroundTriggers data = UndergroundAreaLoader.m_JsonData;
		if (!data || !data.Triggers)
			return;

		s_BunkerCache = new array<ref dmBunkerBox>();

		int i;
		for (i = 0; i < data.Triggers.Count(); i++)
		{
			JsonUndergroundAreaTriggerData t = data.Triggers[i];
			if (!t)
				continue;
			if (t.EyeAccommodation >= 1.0)
				continue;

			dmBunkerBox box = new dmBunkerBox();
			box.m_Center = t.GetPosition();
			vector size = t.GetSize();
			box.m_HalfSize = size * 0.5;
			vector ori = t.GetOrientation();
			vector mat[3];
			Math3D.YawPitchRollMatrix(ori, mat);
			//! Матрица ортонормирована → обратная = транспонированная: строка i
			//! обратной = столбец i исходной. Транспозиция убирает неоднозначность
			//! in-place/row-конвенции MatrixInverse3.
			box.m_InvR0 = Vector(mat[0][0], mat[1][0], mat[2][0]);
			box.m_InvR1 = Vector(mat[0][1], mat[1][1], mat[2][1]);
			box.m_InvR2 = Vector(mat[0][2], mat[1][2], mat[2][2]);
			s_BunkerCache.Insert(box);
		}
	}

	//! Point-in-OBB: локальная точка = inverse-rotation(точка - центр), затем
	//! AABB-тест по половинным размерам. Матрица уже обратная (строки в box).
	private bool PointInBox(dmBunkerBox box, vector pos)
	{
		vector d = pos - box.m_Center;
		float lx = box.m_InvR0[0] * d[0] + box.m_InvR0[1] * d[1] + box.m_InvR0[2] * d[2];
		float ly = box.m_InvR1[0] * d[0] + box.m_InvR1[1] * d[1] + box.m_InvR1[2] * d[2];
		float lz = box.m_InvR2[0] * d[0] + box.m_InvR2[1] * d[1] + box.m_InvR2[2] * d[2];

		if (Math.AbsFloat(lx) > box.m_HalfSize[0])
			return false;
		if (Math.AbsFloat(ly) > box.m_HalfSize[1])
			return false;
		if (Math.AbsFloat(lz) > box.m_HalfSize[2])
			return false;
		return true;
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
