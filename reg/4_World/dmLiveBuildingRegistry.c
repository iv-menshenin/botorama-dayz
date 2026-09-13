//! dmLiveBuildingRegistry — живой реестр застримленных зданий (синглтон).
//!
//! Дома (House) регистрируются в конструкторе (отложенно — через CallLater, потому
//! что в конструкторе у здания ещё нет позиции) и снимаются в деструкторе. Бот
//! использует реестр для лутинга/кочёвки: ближайшее здание нужного типа и число
//! лутабельных зданий рядом.

class dmLiveBuildingRegistry
{
	static ref dmLiveBuildingRegistry s_Instance;

	ref array<Building> m_Buildings;

	static dmLiveBuildingRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new dmLiveBuildingRegistry();
		return s_Instance;
	}

	void dmLiveBuildingRegistry()
	{
		m_Buildings = new array<Building>();
	}

	void Register(Building building)
	{
		if (!building)
			return;
		if (m_Buildings.Find(building) < 0)
			m_Buildings.Insert(building);
	}

	void Unregister(Building building)
	{
		if (!building)
			return;
		m_Buildings.RemoveItem(building);
	}

	Building GetNearest(dmWorldPOIType type, vector pos, float radius)
	{
		Building nearest;
		float nearestDist;
		bool hasNearest;

		vector pos2D = pos;
		pos2D[1] = 0.0;

		int i;
		for (i = 0; i < m_Buildings.Count(); i++)
		{
			Building b = m_Buildings[i];
			if (!b || !b.IsAlive())
				continue;

			if (dmBuildingInteriorMap.Get().GetType(b.GetType()) != type)
				continue;

			vector bPos = b.GetPosition();
			bPos[1] = 0.0;
			float d = vector.Distance(pos2D, bPos);
			if (d > radius)
				continue;

			if (!hasNearest || d < nearestDist)
			{
				nearest = b;
				nearestDist = d;
				hasNearest = true;
			}
		}

		return nearest;
	}

	int CountLootableNear(vector pos, float radius)
	{
		int count;

		vector pos2D = pos;
		pos2D[1] = 0.0;

		int i;
		for (i = 0; i < m_Buildings.Count(); i++)
		{
			Building b = m_Buildings[i];
			if (!b || !b.IsAlive())
				continue;

			if (!dmBuildingInteriorMap.Get().HasPoints(b.GetType()))
				continue;

			vector bPos = b.GetPosition();
			bPos[1] = 0.0;
			if (vector.Distance(pos2D, bPos) <= radius)
				count++;
		}

		return count;
	}
}
