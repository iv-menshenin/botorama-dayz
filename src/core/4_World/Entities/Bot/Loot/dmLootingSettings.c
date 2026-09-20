//! dmLootingSettings — ленивый синглтон настроек лутинга/исследования (looting.json).
//!
//! Читает $profile:dmBotorama/settings/looting.json (dmLootingConfig). При первом
//! запуске, если файла нет, создаёт его с дефолтами (Defaults + Save). Обёртка
//! dmJsonFile<T> даёт версионирование (dmJsonConfigBase).

class dmLootingSettings
{
	static ref dmLootingSettings s_Instance;

	ref dmLootingConfig m_Config;

	static dmLootingSettings Get()
	{
		if (!s_Instance)
		{
			s_Instance = new dmLootingSettings();
			s_Instance.Load();
		}
		return s_Instance;
	}

	//! Загрузить looting.json. Различает «файла нет» (автосоздание с дефолтами) и
	//! «файл есть, но не парсится» (лог ошибки + дефолты в памяти, файл НЕ трогаем).
	void Load()
	{
		dmJsonFile<dmLootingConfig> reader = new dmJsonFile<dmLootingConfig>(DM_LOOTING_SETTINGS_FILE);
		dmLootingConfig config;
		if (reader.Load(config))
		{
			m_Config = config;
			return;
		}
		if (!FileExist(DM_LOOTING_SETTINGS_FILE))
		{
			config = new dmLootingConfig();
			config.Defaults();
			EnsureDirectory(DM_LOOTING_SETTINGS_FILE);
			reader.Save(config);
		}
		else
		{
			dmBotLog.Error("[LootingSettings] read error " + DM_LOOTING_SETTINGS_FILE + ": " + reader.Errors());
			config = new dmLootingConfig();
			config.Defaults();
		}
		m_Config = config;
	}

	dmExplorationConfig GetExploration()
	{
		if (!m_Config || !m_Config.Exploration)
			return null;
		return m_Config.Exploration;
	}

	//! Создать цепочку родительских каталогов пути файла (локальная копия
	//! dmJsonFile.EnsureDirectory: статик generic-класса на месте вызова ненадёжен).
	static void EnsureDirectory(string path)
	{
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash < 0)
			return;

		TStringArray comps = new TStringArray();
		path.Substring(0, lastSlash).Split("/", comps);

		int startFrom = 0;
		string dir = "";
		if (comps.Count() > 0 && comps[0] == "$profile:")
		{
			dir = "$profile:";
			startFrom = 1;
		}

		int i;
		for (i = startFrom; i < comps.Count(); i++)
		{
			if (dir != "")
				dir += "/";
			dir += comps[i];
			if (!FileExist(dir))
				MakeDirectory(dir);
		}
	}
}
