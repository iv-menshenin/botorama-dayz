//! dmNoiseSystem — shared noise signal for bot hearing.
//!
//! The native NoiseSystem is a sink for native zombie/animal sensing and never
//! triggers script callbacks, so the mod publishes its own noise signal here (like
//! Expansion's eAINoiseSystem). Noise generators (gunshots, footsteps, bullet
//! impacts, zombie cries) will call AddNoise in Phase 2; receivers subscribe to
//! SI_OnNoiseAdded (see dmHearing).

enum dmNoiseType
{
	OTHER = 0,
	STEP,
	SCREAM,
	BULLETIMPACT,
	SHOT,
	EXPLOSION,
	DROP,
	SOUND
}

class dmNoiseSystem
{
	static ref ScriptInvoker SI_OnNoiseAdded = new ScriptInvoker;

	//! Локализует ли шум источник: координата шума == координата источника.
	//! true — STEP/SCREAM/SHOT/SOUND/OTHER (звук издан в точке источника);
	//! false — BULLETIMPACT/DROP/EXPLOSION (координата = место эффекта, не источник).
	//! Контракт расширения: новые НЕпозиционные типы добавлять сюда явным return false —
	//! default=true (молчаливый «позиционный» фолбэк) опасен для не-локализующего типа.
	static bool LocalizesSource(int type)
	{
		if (type == dmNoiseType.BULLETIMPACT) return false;
		if (type == dmNoiseType.DROP) return false;
		if (type == dmNoiseType.EXPLOSION) return false;
		return true;
	}

	//! Погодный фактор ослабления шума: 1.0 ясно, меньше — дождь/снег/туман/ветер/буря.
	static float GetWeatherFactor(vector pos)
	{
		Weather w = g_Game.GetWeather();
		float reduction = 0.0;
		reduction = Math.Max(reduction, w.GetRain().GetActual() * DM_WEATHER_RAIN_REDUCTION);
		reduction = Math.Max(reduction, w.GetSnowfall().GetActual() * DM_WEATHER_SNOW_REDUCTION);
		reduction = Math.Max(reduction, w.GetFog().GetActual() * DM_WEATHER_FOG_REDUCTION);
		reduction = Math.Max(reduction, Math.Min(w.GetWindSpeed() / DM_WEATHER_WIND_SPEED_MAX, 1.0) * DM_WEATHER_WIND_REDUCTION);
		Sandstorm ss = w.GetSandstorm();
		if (ss && ss.IsActive())
			reduction = Math.Max(reduction, ss.GetIntensity(pos) * DM_WEATHER_SANDSTORM_REDUCTION);
		return 1.0 - Math.Min(reduction, 1.0);
	}

	//! Сгенерировать шум (source может быть null — шум с координатой, но без источника, напр. попадание пули).
	static void AddNoise(EntityAI source, vector position, float strength, int type = dmNoiseType.OTHER)
	{
		float weatherFactor = GetWeatherFactor(position);
		strength = strength * weatherFactor;
		#ifdef DM_PERCEPTION_DEBUG
		string src = "null";
		if (source) src = source.GetType();
		dmBotLog.Debug("", "[Noise] AddNoise type=" + type + " src=" + src + " pos=" + position + " strength=" + strength + " weather=" + weatherFactor);
		#endif
		SI_OnNoiseAdded.Invoke(source, position, strength, type);
	}
};
