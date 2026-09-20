//! dmWorldPOIRegistry — реестр локаций карты (синглтон, ленивая загрузка).
//!
//! Грузит world_poi.json (dmWorldPoiConfig) и индексирует поселения — локации
//! типа Capital/City/Village/Camp — для спавн-менеджера (query-API). Данные
//! читаются из $profile:dmBotorama/map/world_poi.json — копию сгенерированного
//! data/map/world_poi.json пользователь кладёт сам.

class dmWorldPOIRegistry
{
	static ref dmWorldPOIRegistry s_Instance;

	ref dmWorldPoiConfig m_Config;
	ref array<ref dmWorldPoiLocation> m_Settlements;
	ref array<ref dmWorldPoiLocation> m_Locations;

	static dmWorldPOIRegistry Get()
	{
		if (!s_Instance)
		{
			s_Instance = new dmWorldPOIRegistry();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void dmWorldPOIRegistry()
	{
		m_Settlements = new array<ref dmWorldPoiLocation>();
		m_Locations = new array<ref dmWorldPoiLocation>();
	}

	void Load()
	{
		dmJsonFile<dmWorldPoiConfig> reader = new dmJsonFile<dmWorldPoiConfig>(DM_MAP_WORLD_POI_FILE);
		dmWorldPoiConfig config;
		if (!reader.Load(config))
		{
			dmBotLog.Error("[POIRegistry] read error " + DM_MAP_WORLD_POI_FILE + ": " + reader.Errors());
			return;
		}
		m_Config = config;

		m_Settlements.Clear();
		m_Locations.Clear();
		int totalLocations = 0;
		if (config.Locations)
		{
			totalLocations = config.Locations.Count();
			int i;
			for (i = 0; i < config.Locations.Count(); i++)
			{
				dmWorldPoiLocation loc = config.Locations[i];
				if (loc)
				{
					loc.Id = m_Locations.Count();
					m_Locations.Insert(loc);
					if (IsSettlementType(loc.Type))
						m_Settlements.Insert(loc);
				}
			}
		}

		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("[POIRegistry] loaded " + totalLocations + " locations, " + m_Settlements.Count() + " settlements");
		#endif
	}

	int SettlementCount()
	{
		return m_Settlements.Count();
	}

	dmWorldPoiLocation GetSettlement(int index)
	{
		if (index < 0 || index >= m_Settlements.Count())
			return null;
		return m_Settlements[index];
	}

	void GetSettlements(out array<ref dmWorldPoiLocation> outSettlements)
	{
		if (!outSettlements)
			outSettlements = new array<ref dmWorldPoiLocation>();

		int i;
		for (i = 0; i < m_Settlements.Count(); i++)
			outSettlements.Insert(m_Settlements[i]);
	}

	dmWorldPoiLocation GetNearestSettlement(vector pos)
	{
		dmWorldPoiLocation nearest;
		float bestDist = 0.0;
		vector locPos;
		vector p;
		float dist;
		int i;

		for (i = 0; i < m_Settlements.Count(); i++)
		{
			dmWorldPoiLocation loc = m_Settlements[i];
			if (loc)
			{
				locPos = loc.Position;
				locPos[1] = 0.0;
				p = pos;
				p[1] = 0.0;
				dist = vector.Distance(locPos, p);
				if (!nearest || dist < bestDist)
				{
					nearest = loc;
					bestDist = dist;
				}
			}
		}
		return nearest;
	}

	static bool IsSettlementType(string type)
	{
		if (type == "Capital")
			return true;
		if (type == "City")
			return true;
		if (type == "Village")
			return true;
		if (type == "Camp")
			return true;
		return false;
	}

	int LocationCount()
	{
		return m_Locations.Count();
	}

	dmWorldPoiLocation GetLocation(int id)
	{
		if (id < 0 || id >= m_Locations.Count())
			return null;
		return m_Locations[id];
	}

	//! Ближайшая локация, в радиусе присутствия которой (по типу) лежит pos.
	//! 2D-дистанция через LengthSq (без sqrt); среди подходящих — наименьшая.
	dmWorldPoiLocation GetLocationAt(vector pos)
	{
		dmWorldPoiLocation nearest;
		float bestSq = 0.0;
		vector locPos;
		vector p;
		float dx;
		float dz;
		float dSq;
		float radius;
		float radiusSq;
		int i;

		for (i = 0; i < m_Locations.Count(); i++)
		{
			dmWorldPoiLocation loc = m_Locations[i];
			if (loc)
			{
				locPos = loc.Position;
				p = pos;
				dx = locPos[0] - p[0];
				dz = locPos[2] - p[2];
				dSq = dx * dx + dz * dz;
				radius = GetPresenceRadius(loc.Type);
				radiusSq = radius * radius;
				if (dSq < radiusSq)
				{
					if (!nearest || dSq < bestSq)
					{
						nearest = loc;
						bestSq = dSq;
					}
				}
			}
		}
		return nearest;
	}

	static float GetPresenceRadius(string type)
	{
		if (type == "Capital")
			return DM_POI_PRESENCE_CAPITAL;
		if (type == "City")
			return DM_POI_PRESENCE_CITY;
		if (type == "Village")
			return DM_POI_PRESENCE_VILLAGE;
		return DM_POI_PRESENCE_OTHER;
	}

	static float GetArrivalRadius(string type)
	{
		if (type == "Capital")
			return DM_POI_ARRIVE_CAPITAL;
		if (type == "City")
			return DM_POI_ARRIVE_CITY;
		if (type == "Village")
			return DM_POI_ARRIVE_VILLAGE;
		return DM_POI_ARRIVE_OTHER;
	}
}
