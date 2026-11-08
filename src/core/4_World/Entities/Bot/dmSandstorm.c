//! dmSandstorm — статик-хелпер над ванильным API песчаной бури (1.30).
//!
//! Единая точка входа «в буре ли точка (географически)»: интенсивность в
//! world-позиции через Sandstorm.GetIntensity(pos), а НЕ IsActive() (буря где-то
//! на карте) и НЕ GetIntensityForPlayer() (учитывает модификаторы укрытий).
//! GetIntensity требует активной бури — всегда гейтим IsActive().
class dmSandstorm
{
	//! true, когда точка pos внутри фронта бури с интенсивностью > threshold.
	static bool IsPointInStorm(vector pos, float threshold)
	{
		Sandstorm s = g_Game.GetWeather().GetSandstorm();
		if (!s || !s.IsActive())
			return false;
		return s.GetIntensity(pos) > threshold;
	}
}
