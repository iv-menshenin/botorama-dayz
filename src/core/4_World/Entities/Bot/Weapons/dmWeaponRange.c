//! dmWeaponRange — статический хелпер «эффективной дистанции» оружия.
//!
//! Базовая дистанция (без оптики) считается по подтверждённой формуле:
//!   WeaponLength(м) × initSpeed(м/с) × initSpeedMultiplier
//! где initSpeed резолвится через chamberableFrom[0] → CfgMagazines → CfgAmmo
//! (переиспользуется dmProjectileCache), а initSpeedMultiplier — через
//! dmGetInitSpeedMultiplier(). Дробовик делится на 2 (разлёт картечи).
//! Значения кэшируются по типу оружия (config читается один раз на тип).
class dmWeaponRange
{
	static ref map<string, float> s_BaseCache = new map<string, float>;

	//! Базовая дистанция (без оптики) по типу оружия, кэшируется.
	static float GetBaseRange(Weapon_Base w)
	{
		if (!w)
			return 0.0;
		string key = w.GetType();
		float cached;
		if (s_BaseCache.Find(key, cached))
			return cached;

		// WeaponLength (гейт на наличие: ConfigGetFloat без поля вернёт дефолт/0)
		float wl;
		if (w.ConfigIsExisting("WeaponLength"))
			wl = w.ConfigGetFloat("WeaponLength");
		else if (w.IsInherited(Pistol_Base))
			wl = 0.2;
		else
			wl = 0.8;

		// initSpeed через chamberableFrom[0] -> CfgMagazines -> CfgAmmo
		float initSpeed = 0.0;
		array<string> chamberable = new array<string>();
		w.ConfigGetTextArray("chamberableFrom", chamberable);
		if (chamberable.Count() > 0)
		{
			dmProjectileInfo p = dmProjectileCache.Get(chamberable[0]);
			if (p)
				initSpeed = p.m_InitSpeed;
		}
		if (initSpeed <= 0.0)
			return 0.0;

		float ism = w.dmGetInitSpeedMultiplier();
		float eff = wl * initSpeed * ism;

		// дробовик: поправка на разлёт дроби — делим на 2
		if (w.IsInherited(Shotgun_Base))
			eff = eff * 0.5;

		s_BaseCache.Insert(key, eff);
		return eff;
	}

	//! Эффективная дистанция с учётом оптики (увеличивающей): подтягиваем к 1000 м.
	static float GetEffectiveRange(Weapon_Base w)
	{
		float eff = GetBaseRange(w);
		if (eff <= 0.0 || w.IsInherited(Shotgun_Base))
			return eff;

		ItemOptics optic = w.GetAttachedOptics();
		bool magnifying = optic && !optic.AllowsDOF();
		if (!magnifying || eff >= 1000.0)
			return eff;

		float gap = 1000.0 - eff;
		float inc = gap * 0.5;
		if (inc > eff)
			inc = eff;
		return eff + inc;
	}

	//! Класс пули (калибр) оружия — резолв chamberableFrom[0] -> CfgMagazines ammo.
	//! Нужен для подсчёта патронов в арсенале (следующая задача).
	static string GetCaliber(Weapon_Base w)
	{
		if (!w)
			return "";
		array<string> chamberable = new array<string>();
		w.ConfigGetTextArray("chamberableFrom", chamberable);
		if (chamberable.Count() == 0)
			return "";
		string bullet;
		if (!g_Game.ConfigGetText(CFG_MAGAZINESPATH + " " + chamberable[0] + " ammo", bullet))
			return "";
		return bullet;
	}
}
