//! dmBuildingInteriorMap — интерьер-карта зданий (синглтон, ленивая загрузка).
//!
//! Держит dmBuildingInteriorConfig и быстрый индекс по классу здания (m_ByClass).
//! Данные читаются из $profile:dmBotorama/map/buildings_interior.json — копию
//! сгенерированного data/map/buildings_interior.json пользователь кладёт сам.
//!
//! Интерьер-точки хранятся в локальном пространстве здания; GetRoamWorldPoints
//! конвертирует их в мир (поворот вокруг Y на яу здания + смещение на позицию).

class dmBuildingInteriorMap
{
	static ref dmBuildingInteriorMap s_Instance;

	ref dmBuildingInteriorConfig m_Config;
	ref map<string, ref dmBuildingInteriorEntry> m_ByClass;

	static dmBuildingInteriorMap Get()
	{
		if (!s_Instance)
		{
			s_Instance = new dmBuildingInteriorMap();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void dmBuildingInteriorMap()
	{
		m_ByClass = new map<string, ref dmBuildingInteriorEntry>();
	}

	void Load()
	{
		dmJsonFile<dmBuildingInteriorConfig> reader = new dmJsonFile<dmBuildingInteriorConfig>(DM_MAP_BUILDINGS_FILE);
		dmBuildingInteriorConfig config;
		if (!reader.Load(config))
		{
			dmBotLog.Error("[InteriorMap] read error " + DM_MAP_BUILDINGS_FILE + ": " + reader.Errors());
			return;
		}
		m_Config = config;

		m_ByClass.Clear();
		if (m_Config.Buildings)
		{
			int i;
			for (i = 0; i < m_Config.Buildings.Count(); i++)
			{
				dmBuildingInteriorEntry entry = m_Config.Buildings[i];
				if (entry && entry.ClassName != "")
					m_ByClass.Set(entry.ClassName, entry);
			}
		}

		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("[InteriorMap] loaded " + m_ByClass.Count() + " building entries");
		#endif
	}

	dmWorldPOIType GetType(string buildingClass)
	{
		dmBuildingInteriorEntry e = m_ByClass.Get(buildingClass);
		if (e)
			return ParseType(e.Type);
		return dmWorldPOIType.NONE;
	}

	bool HasPoints(string buildingClass)
	{
		dmBuildingInteriorEntry e = m_ByClass.Get(buildingClass);
		if (!e || !e.Points)
			return false;
		return e.Points.Count() > 0;
	}

	void GetPoints(string buildingClass, out array<vector> outPoints)
	{
		if (!outPoints)
			outPoints = new array<vector>();

		dmBuildingInteriorEntry e = m_ByClass.Get(buildingClass);
		if (!e || !e.Points)
			return;

		int i;
		for (i = 0; i < e.Points.Count(); i++)
			outPoints.Insert(e.Points[i]);
	}

	void GetRoamWorldPoints(Building building, out array<vector> outWorld)
	{
		if (!outWorld)
			outWorld = new array<vector>();

		if (!building)
			return;

		dmBuildingInteriorEntry e = m_ByClass.Get(building.GetType());
		if (!e || !e.Points)
			return;

		vector pos = building.GetPosition();
		vector orient = building.GetOrientation();
		float yaw = orient[0] * Math.DEG2RAD;
		float c = Math.Cos(yaw);
		float s = Math.Sin(yaw);

		int i;
		for (i = 0; i < e.Points.Count(); i++)
		{
			vector r = e.Points[i];
			float wx = pos[0] + (r[0] * c - r[2] * s);
			float wy = pos[1] + r[1];
			float wz = pos[2] + (r[0] * s + r[2] * c);
			vector worldPoint;
			worldPoint[0] = wx;
			worldPoint[1] = wy;
			worldPoint[2] = wz;
			outWorld.Insert(worldPoint);
		}
	}

	static dmWorldPOIType ParseType(string s)
	{
		if (s == "WATER") return dmWorldPOIType.WATER;
		if (s == "POLICE") return dmWorldPOIType.POLICE;
		if (s == "FIRE") return dmWorldPOIType.FIRE;
		if (s == "MEDICAL") return dmWorldPOIType.MEDICAL;
		if (s == "MILITARY") return dmWorldPOIType.MILITARY;
		if (s == "MILITARY_WRECK") return dmWorldPOIType.MILITARY_WRECK;
		if (s == "FUEL") return dmWorldPOIType.FUEL;
		if (s == "INDUSTRIAL") return dmWorldPOIType.INDUSTRIAL;
		if (s == "RESIDENTIAL") return dmWorldPOIType.RESIDENTIAL;
		if (s == "GENERIC") return dmWorldPOIType.GENERIC;
		return dmWorldPOIType.NONE;
	}
}
