//! dmFireMode — read an AI weapon's fire modes from config and classify them
//! (Phase 2a).
//!
//! Modes come from CfgWeapons <type> modes[] and are classified into
//! dmFireModeType (single/burst/auto/double), each carrying its burst/dispersion/
//! reloadTime from config. dmWeaponFireInfo caches the parsed modes per weapon
//! type (like ExpansionWeaponInfo), so config is read once per type. The pawn
//! (dmAISurvivorBase) exposes the selection helpers on top of this cache.

//! Тип режима огня (для выбора дистанции).
enum dmFireModeType
{
	SINGLE = 0,
	BURST,
	AUTO,
	DOUBLE
}

//! Один режим огня оружия (из конфига modes[]).
class dmFireMode
{
	int m_Index;           // индекс в modes[]
	int m_Type;            // dmFireModeType
	int m_Burst;           // burst из конфига (1 для одиночного)
	float m_Dispersion;    // dispersion из конфига
	float m_ReloadTime;    // reloadTime из конфига
}

//! Кэш режимов огня по типу оружия (статический, как ExpansionWeaponInfo).
class dmWeaponFireInfo
{
	static ref map<string, ref dmWeaponFireInfo> s_Cache = new map<string, ref dmWeaponFireInfo>;

	ref array<ref dmFireMode> m_Modes;

	void dmWeaponFireInfo(Weapon_Base weapon)
	{
		m_Modes = new array<ref dmFireMode>();
		TStringArray modes = new TStringArray();
		weapon.ConfigGetTextArray("modes", modes);
		int i;
		for (i = 0; i < modes.Count(); i++)
		{
			dmFireMode mode = new dmFireMode();
			mode.m_Index = i;
			mode.m_Type = ClassifyModeName(modes[i]);
			mode.m_Burst = ReadBurst(weapon, modes[i]);
			mode.m_Dispersion = ReadDispersion(weapon, modes[i]);
			mode.m_ReloadTime = ReadReloadTime(weapon, modes[i]);
			m_Modes.Insert(mode);
		}

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FireMode] " + weapon.GetType() + " modes=" + modes.Count());
		#endif
	}

	//! Получить (или создать и закэшировать) инфо по оружию.
	static dmWeaponFireInfo Get(Weapon_Base weapon)
	{
		string type = weapon.GetType();
		dmWeaponFireInfo info;
		if (s_Cache.Find(type, info))
			return info;
		info = new dmWeaponFireInfo(weapon);
		s_Cache.Insert(type, info);
		return info;
	}

	//! Классификация имени режима по строке.
	private int ClassifyModeName(string name)
	{
		if (name.Contains("Double"))
			return dmFireModeType.DOUBLE;
		if (name.Contains("Burst"))
			return dmFireModeType.BURST;
		if (name.Contains("FullAuto"))
			return dmFireModeType.AUTO;
		return dmFireModeType.SINGLE;
	}

	private int ReadBurst(Weapon_Base weapon, string mode)
	{
		int burst = g_Game.ConfigGetInt(CFG_WEAPONSPATH + " " + weapon.GetType() + " " + mode + " burst");
		if (burst < 1)
			burst = 1;
		return burst;
	}

	private float ReadDispersion(Weapon_Base weapon, string mode)
	{
		return g_Game.ConfigGetFloat(CFG_WEAPONSPATH + " " + weapon.GetType() + " " + mode + " dispersion");
	}

	private float ReadReloadTime(Weapon_Base weapon, string mode)
	{
		return g_Game.ConfigGetFloat(CFG_WEAPONSPATH + " " + weapon.GetType() + " " + mode + " reloadTime");
	}
}
