//! dmLiveBuildingRegistry — живой реестр застримленных зданий (синглтон).
//!
//! Дома (House) регистрируются в конструкторе (отложенно — через CallLater, потому
//! что в конструкторе у здания ещё нет позиции) и снимаются в деструкторе. Каждое
//! здание привязывается к ближайшей локации (dmWorldPOIRegistry.GetLocationAt) —
//! m_ByLocation индексирует здания по locationId для обхода в рамках одной локации.
//! Бот использует реестр для лутинга/кочёвки: ближайшее здание нужного типа, число
//! лутабельных зданий рядом, здания конкретной локации.

//! Обёртка зарегистрированного здания: ссылка + привязка к локации (-1 = вне локаций).
class dmRegisteredBuilding
{
	Building m_Building;   // managed, без ref
	int m_LocationId;      // -1 = вне локаций
}

class dmLiveBuildingRegistry
{
	static ref dmLiveBuildingRegistry s_Instance;

	ref array<ref dmRegisteredBuilding> m_Buildings;   // плоский список (обёртки)
	ref map<int, ref array<Building>> m_ByLocation;    // locationId -> здания
	ref TStringArray m_Excluded;                       // префиксы исключённых типов зданий

	static dmLiveBuildingRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new dmLiveBuildingRegistry();
		return s_Instance;
	}

	void dmLiveBuildingRegistry()
	{
		m_Buildings = new array<ref dmRegisteredBuilding>();
		m_ByLocation = new map<int, ref array<Building>>();
		m_Excluded = new TStringArray();

		m_Excluded.Insert("Land_Boat_");
		m_Excluded.Insert("Land_CementWorks_Hall2_Grey");
		m_Excluded.Insert("Land_Factory_Small");
		m_Excluded.Insert("Land_House_1W09");
		m_Excluded.Insert("Land_House_2W03");
		m_Excluded.Insert("Land_HouseBlock_1F4");
		m_Excluded.Insert("Land_Boathouse");
		m_Excluded.Insert("Land_Mine_Building");
		m_Excluded.Insert("Land_Shed_W2");
		m_Excluded.Insert("Land_Tenement_Big");
		m_Excluded.Insert("Land_Misc_Toilet_Mobile");
		m_Excluded.Insert("Land_Ship_Medium2");
		m_Excluded.Insert("Land_Train_Wagon_Box");
	}

	void Register(Building building)
	{
		if (!building)
			return;
		if (IsExcludedBuilding(building))
			return;

		int i;
		for (i = 0; i < m_Buildings.Count(); i++)
		{
			if (m_Buildings[i].m_Building == building)
				return;
		}

		dmWorldPoiLocation loc = dmWorldPOIRegistry.Get().GetLocationAt(building.GetPosition());
		int id = -1;
		if (loc)
			id = loc.Id;

		dmRegisteredBuilding rec = new dmRegisteredBuilding();
		rec.m_Building = building;
		rec.m_LocationId = id;
		m_Buildings.Insert(rec);

		if (id >= 0)
		{
			array<Building> arr = m_ByLocation.Get(id);
			if (!arr)
			{
				arr = new array<Building>();
				m_ByLocation.Set(id, arr);
			}
			arr.Insert(building);
		}
	}

	void Unregister(Building building)
	{
		if (!building)
			return;

		int i;
		for (i = 0; i < m_Buildings.Count(); i++)
		{
			dmRegisteredBuilding rec = m_Buildings[i];
			if (rec.m_Building == building)
			{
				if (rec.m_LocationId >= 0)
				{
					array<Building> arr = m_ByLocation.Get(rec.m_LocationId);
					if (arr)
					{
						arr.RemoveItem(building);
						if (arr.Count() == 0)
							m_ByLocation.Remove(rec.m_LocationId);
					}
				}
				m_Buildings.Remove(i);
				return;
			}
		}
	}

	//! Скопировать живые здания локации в out (или пусто).
	void GetBuildingsForLocation(dmWorldPoiLocation loc, out array<Building> outBuildings)
	{
		if (!outBuildings)
			outBuildings = new array<Building>();

		if (!loc)
			return;

		array<Building> arr = m_ByLocation.Get(loc.Id);
		if (!arr)
			return;

		int i;
		for (i = 0; i < arr.Count(); i++)
		{
			Building b = arr[i];
			if (!b || !b.IsAlive())
				continue;
			outBuildings.Insert(b);
		}
	}

	//! Ближайшее живое здание с дверьми (BuildingBase + GetDoorCount() > 0) в
	//! радиусе radius, не в списке exclude. 2D-дистанция LengthSq.
	Building GetNearestBuildingWithDoors(vector pos, float radius, array<Building> exclude)
	{
		Building nearest;
		float nearestSq = 0.0;
		vector pos2D = pos;
		pos2D[1] = 0.0;
		vector bPos;
		float dx;
		float dz;
		float dSq;
		float radiusSq = radius * radius;
		int i;

		for (i = 0; i < m_Buildings.Count(); i++)
		{
			Building b = m_Buildings[i].m_Building;
			if (!b || !b.IsAlive())
				continue;

			BuildingBase base = BuildingBase.Cast(b);
			if (!base || base.GetDoorCount() == 0)
				continue;

			if (exclude && exclude.Find(b) >= 0)
				continue;

			bPos = b.GetPosition();
			dx = bPos[0] - pos2D[0];
			dz = bPos[2] - pos2D[2];
			dSq = dx * dx + dz * dz;
			if (dSq > radiusSq)
				continue;

			if (!nearest || dSq < nearestSq)
			{
				nearest = b;
				nearestSq = dSq;
			}
		}

		return nearest;
	}

	Building GetNearest(dmWorldPOIType type, vector pos, float radius)
	{
		Building nearest;
		float nearestSq = 0.0;
		vector pos2D = pos;
		pos2D[1] = 0.0;
		vector bPos;
		float dx;
		float dz;
		float dSq;
		float radiusSq = radius * radius;
		int i;

		for (i = 0; i < m_Buildings.Count(); i++)
		{
			Building b = m_Buildings[i].m_Building;
			if (!b || !b.IsAlive())
				continue;

			if (dmBuildingInteriorMap.Get().GetType(b.GetType()) != type)
				continue;

			bPos = b.GetPosition();
			dx = bPos[0] - pos2D[0];
			dz = bPos[2] - pos2D[2];
			dSq = dx * dx + dz * dz;
			if (dSq > radiusSq)
				continue;

			if (!nearest || dSq < nearestSq)
			{
				nearest = b;
				nearestSq = dSq;
			}
		}

		return nearest;
	}

	int CountLootableNear(vector pos, float radius)
	{
		int count;
		vector pos2D = pos;
		pos2D[1] = 0.0;
		vector bPos;
		float dx;
		float dz;
		float dSq;
		float radiusSq = radius * radius;
		int i;

		for (i = 0; i < m_Buildings.Count(); i++)
		{
			Building b = m_Buildings[i].m_Building;
			if (!b || !b.IsAlive())
				continue;

			if (!dmBuildingInteriorMap.Get().HasPoints(b.GetType()))
				continue;

			bPos = b.GetPosition();
			dx = bPos[0] - pos2D[0];
			dz = bPos[2] - pos2D[2];
			dSq = dx * dx + dz * dz;
			if (dSq <= radiusSq)
				count++;
		}

		return count;
	}

	//! Тип здания исключён из привязки/обхода (префикс-совпадение с m_Excluded).
	bool IsExcludedBuilding(Building building)
	{
		string buildingType = building.GetType();
		int i;
		for (i = 0; i < m_Excluded.Count(); i++)
		{
			if (buildingType.IndexOf(m_Excluded[i]) == 0)
				return true;
		}
		return false;
	}
}
