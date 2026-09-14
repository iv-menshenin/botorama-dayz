//! dmProjectileCache — cached ballistic attributes of the chambered bullet.
//!
//! Resolved lazily: CfgMagazines <ammoType> ammo -> bullet type name, then
//! CfgAmmo <bullet> {initSpeed, airFriction, weight, caliber}. Cached per
//! ammo type (map keyed by cartridge name, like dmWeaponFireInfo.s_Cache) so
//! config is read once per cartridge instead of once per shot.

//! Кэшированные характеристики пули (проектиля), читаются из CfgAmmo <bullet>.
class dmProjectileInfo
{
	float m_InitSpeed;    // CfgAmmo <bullet> initSpeed
	float m_AirFriction;  // CfgAmmo <bullet> airFriction
	float m_Weight;       // CfgAmmo <bullet> weight (кг)
	float m_Caliber;      // CfgAmmo <bullet> caliber
}

//! Глобальный кэш характеристик пули по ключу — имя патрона (напр. "Ammo_762x54").
//! Заполняется лениво: CfgMagazines <патрон> ammo -> имя пули, затем
//! CfgAmmo <пуля> атрибуты. Паттерн — как dmWeaponFireInfo.s_Cache.
class dmProjectileCache
{
	static ref map<string, ref dmProjectileInfo> s_Cache = new map<string, ref dmProjectileInfo>;

	static dmProjectileInfo Get(string ammoType)
	{
		if (ammoType == "")
			return null;
		dmProjectileInfo info = s_Cache[ammoType];
		if (info)
			return info;
		string bullet;
		if (!g_Game.ConfigGetText(CFG_MAGAZINESPATH + " " + ammoType + " ammo", bullet))
			return null;
		if (bullet == "")
			return null;
		info = new dmProjectileInfo();
		info.m_InitSpeed = g_Game.ConfigGetFloat(CFG_AMMO + " " + bullet + " initSpeed");
		info.m_AirFriction = g_Game.ConfigGetFloat(CFG_AMMO + " " + bullet + " airFriction");
		info.m_Weight = g_Game.ConfigGetFloat(CFG_AMMO + " " + bullet + " weight");
		info.m_Caliber = g_Game.ConfigGetFloat(CFG_AMMO + " " + bullet + " caliber");
		s_Cache[ammoType] = info;
		return info;
	}
}
