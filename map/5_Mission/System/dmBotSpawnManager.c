//! dmBotSpawnManager — спавн ботов «по городам» и контроль их числа (синглтон, ленивая инициализация).
//!
//! Читает spawn.json (dmSpawnConfig) и строит список записей (dmBotSpawnEntry) из
//! реестра поселений dmWorldPOIRegistry: на каждое поселение — квота ботов (override
//! из config.Settlements или общий BotsPerSettlement). Тикер (Tick) вызывается из
//! MissionServer.OnUpdate и держит популяцию: спавнит по квоте, кап MaxBots, респавн
//! после RespawnDelay. Данные читаются из $profile:dmBotorama/Map/spawn.json — файл
//! по умолчанию создаётся при первом запуске, если его нет.

//! Одна запись спавна: бот привязан к поселению, либо жив, либо ждёт респавна.
class dmBotSpawnEntry
{
	string Settlement;         // имя локации
	vector Position;           // [x, y, z] — где спавнить
	ref dmAISurvivor Bot;      // null если не заспавнен/мёртв
	float RespawnTimer;        // обратный отсчёт после смерти
}

class dmBotSpawnManager
{
	static ref dmBotSpawnManager s_Instance;

	ref dmSpawnConfig m_Config;
	ref array<ref dmBotSpawnEntry> m_Entries;
	float m_TickAccum;         // троттлинг тика

	static dmBotSpawnManager Get()
	{
		if (!s_Instance)
		{
			s_Instance = new dmBotSpawnManager();
			s_Instance.Init();
		}
		return s_Instance;
	}

	void dmBotSpawnManager()
	{
		m_Entries = new array<ref dmBotSpawnEntry>();
	}

	//! Загрузить spawn.json (Defaults + Save, если файла нет) и построить m_Entries
	//! из реестра поселений.
	void Init()
	{
		dmJsonFile<dmSpawnConfig> reader = new dmJsonFile<dmSpawnConfig>(DM_MAP_SPAWN_FILE);
		dmSpawnConfig config;
		if (!reader.Load(config))
		{
			config = new dmSpawnConfig();
			config.Defaults();
			EnsureDirectory(DM_MAP_SPAWN_FILE);
			reader.Save(config);
		}
		m_Config = config;

		m_Entries.Clear();
		dmWorldPOIRegistry registry = dmWorldPOIRegistry.Get();
		int settlements = registry.SettlementCount();
		int entryCount = 0;
		int i;
		for (i = 0; i < settlements; i++)
		{
			dmWorldPoiLocation settlement = registry.GetSettlement(i);
			if (!settlement)
				continue;

			int quota = m_Config.BotsPerSettlement;
			dmSpawnSettlement overrideCfg = FindSettlement(settlement.Name);
			if (overrideCfg)
				quota = overrideCfg.Count;

			int j;
			for (j = 0; j < quota; j++)
			{
				dmBotSpawnEntry entry = new dmBotSpawnEntry();
				entry.Settlement = settlement.Name;
				entry.Position = RollSpawnPosition(settlement.Position);
				entry.Bot = null;
				entry.RespawnTimer = 0.0;
				m_Entries.Insert(entry);
				entryCount++;
			}
		}

		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("[SpawnManager] init: settlements=" + settlements + " entries=" + entryCount);
		#endif
	}

	//! Тик спавн-менеджера (троттлинг DM_SPAWN_TICK_INTERVAL). Спавнит по квоте,
	//! держит кап MaxBots и запускает респавн умерших после RespawnDelay.
	void Tick(float pDt)
	{
		m_TickAccum += pDt;
		if (m_TickAccum < DM_SPAWN_TICK_INTERVAL)
			return;
		m_TickAccum = 0.0;

		if (!m_Config || !m_Config.Enabled)
			return;

		if (dmAISurvivor.Count() >= m_Config.MaxBots)
			return;

		int i;
		for (i = 0; i < m_Entries.Count(); i++)
		{
			dmBotSpawnEntry entry = m_Entries[i];
			if (!entry)
				continue;

			if (entry.Bot && !entry.Bot.IsSpawned())
			{
				entry.Bot = null;
				entry.RespawnTimer = m_Config.RespawnDelay;
			}

			if (!entry.Bot)
			{
				entry.RespawnTimer -= DM_SPAWN_TICK_INTERVAL;
				if (entry.RespawnTimer <= 0.0)
					SpawnEntry(entry);
			}
		}
	}

	//! Заспавнить бота в entry (модель случайная, loadout из config, FSM Survivor).
	void SpawnEntry(dmBotSpawnEntry entry)
	{
		ref dmAISurvivor bot = new dmAISurvivor();
		bot.SetModel(dmSurvivor.GetRandom());
		PlayerBase pawn = bot.Spawn(entry.Position, Vector(Math.RandomFloat(0.0, 360.0), 0.0, 0.0));
		if (!pawn)
		{
			#ifdef DM_BOT_DEBUG_SPAWN
			dmBotLog.Debug("[SpawnManager] spawn FAILED: settlement=" + entry.Settlement + " pos=" + entry.Position);
			#endif
			return;
		}

		dmLoadoutConfig cfg = dmLoadoutApplier.Load(m_Config.SpawnLoadout);
		if (cfg)
			dmLoadoutApplier.Apply(pawn, cfg);

		bot.SetFSM(dmBotPreset_Survivor.Create(bot));
		entry.Bot = bot;

		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("[SpawnManager] spawned: settlement=" + entry.Settlement + " pos=" + entry.Position + " total=" + dmAISurvivor.Count());
		#endif
	}

	//! Спавнит все незаполненные слоты немедленно (ручной триггер из чата).
	void SpawnAll()
	{
		int i;
		for (i = 0; i < m_Entries.Count(); i++)
		{
			dmBotSpawnEntry entry = m_Entries[i];
			if (entry && !entry.Bot)
				SpawnEntry(entry);
		}
	}

	//! Прибить точку строго к земле (SurfaceY по x/z), игнорируя pos[1].
	static vector SnapToGround(vector pos)
	{
		float posX = pos[0];
		float posZ = pos[2];
		float y = GetGame().SurfaceY(posX, posZ);
		return Vector(posX, y, posZ);
	}

	//! Случайная позиция спавна в пределах ±DM_SPAWN_CITY_OFFSET от центра поселения.
	vector RollSpawnPosition(vector settlementPos)
	{
		float posX = settlementPos[0];
		float posZ = settlementPos[2];
		float offX = Math.RandomFloat(-DM_SPAWN_CITY_OFFSET, DM_SPAWN_CITY_OFFSET);
		float offZ = Math.RandomFloat(-DM_SPAWN_CITY_OFFSET, DM_SPAWN_CITY_OFFSET);
		vector candidate = Vector(posX + offX, 0.0, posZ + offZ);
		return SnapToGround(candidate);
	}

	//! Override квоты для поселения по имени (null — нет).
	dmSpawnSettlement FindSettlement(string name)
	{
		if (!m_Config || !m_Config.Settlements)
			return null;

		int i;
		for (i = 0; i < m_Config.Settlements.Count(); i++)
		{
			dmSpawnSettlement s = m_Config.Settlements[i];
			if (s && s.Name == name)
				return s;
		}
		return null;
	}

	//! Создать цепочку родительских каталогов пути файла (аналог dmJsonFile.EnsureDirectory).
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
