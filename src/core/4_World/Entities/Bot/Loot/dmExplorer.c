//! dmExplorer — память исследования: какие локации бот посетил и какие здания
//! текущей локации обошёл.
//!
//! Здания больше не сканируются радиус-боксом: источник зданий — реестр
//! dmLiveBuildingRegistry (привязка зданий к локациям через dmWorldPOIRegistry).
//! При входе в локацию (ArriveAtLocation) explorer берёт здания локации из
//! реестра и дальше ведёт только флаг «посещено» по каждому. Локации помечаются
//! посещёнными по Id (m_VisitedLocations). Никого не пишет, ссылок на
//! Wishlist/Requirements/dmNeeds нет.

//! Здание в памяти исследования: ссылка + флаг «посещено».
class dmExploredBuilding
{
	Building m_Building;
	bool m_Visited;
};

class dmExplorer
{
	ref array<int> m_VisitedLocations;                       // Id посещённых локаций
	ref dmWorldPoiLocation m_CurrentLocation;                // текущая локация (null = вне)
	ref dmWorldPoiLocation m_Destination;                    // назначение кочёвки (веха E)
	ref array<ref dmExploredBuilding> m_LocationBuildings;   // здания текущей локации
	bool m_NothingToDo;                                      // «делать больше нечего» (веха D)
	bool m_InTransit;                                        // «переход в локацию» (веха E)
	float m_TimeInLocation;                                  // время в текущей локации (веха D)

	void dmExplorer()
	{
		m_VisitedLocations = new array<int>();
		m_LocationBuildings = new array<ref dmExploredBuilding>();
		m_CurrentLocation = null;
		m_Destination = null;
		m_NothingToDo = false;
		m_InTransit = false;
		m_TimeInLocation = 0.0;
	}

	//! Посещал ли бот локацию с данным Id.
	bool HasVisited(int id)
	{
		return m_VisitedLocations.Find(id) >= 0;
	}

	//! Пометить локацию посещённой (идемпотентно).
	void RememberLocation(int id)
	{
		if (!HasVisited(id))
			m_VisitedLocations.Insert(id);
	}

	//! Вход в локацию: запомнить, взять её здания из реестра и сбросить прогресс.
	//! Идемпотентно — повторный вход в ту же локацию ничего не делает.
	void ArriveAtLocation(dmWorldPoiLocation loc)
	{
		if (!loc)
			return;
		if (m_CurrentLocation && m_CurrentLocation.Id == loc.Id)
			return;

		m_CurrentLocation = loc;
		RememberLocation(loc.Id);

		m_LocationBuildings.Clear();
		array<Building> buildings = new array<Building>();
		dmLiveBuildingRegistry.Get().GetBuildingsForLocation(loc, buildings);
		int i;
		for (i = 0; i < buildings.Count(); i++)
		{
			dmExploredBuilding eb = new dmExploredBuilding();
			eb.m_Building = buildings[i];
			eb.m_Visited = false;
			m_LocationBuildings.Insert(eb);
		}

		m_TimeInLocation = 0.0;
		SetNothingToDo(false);

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Explorer: прибыл в локацию " + loc.Id + " (" + loc.Name + "), зданий " + m_LocationBuildings.Count());
		#endif
	}

	//! Ближайшее непосещённое здание текущей локации (2D до позиции бота), или null.
	Building GetNextUnvisitedBuilding(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return null;

		vector botPos = pawn.GetPosition();
		Building best = null;
		float bestSq = 0.0;
		vector bPos;
		float dSq;
		int i;
		for (i = 0; i < m_LocationBuildings.Count(); i++)
		{
			dmExploredBuilding eb = m_LocationBuildings[i];
			if (eb.m_Visited || !eb.m_Building)
				continue;
			bPos = eb.m_Building.GetPosition();
			dSq = dmMath.DistSq2D(bPos, botPos);
			if (!best || dSq < bestSq)
			{
				best = eb.m_Building;
				bestSq = dSq;
			}
		}
		return best;
	}

	//! Пометить здание текущей локации посещённым.
	void MarkLocationBuildingVisited(Building building)
	{
		int i;
		for (i = 0; i < m_LocationBuildings.Count(); i++)
		{
			dmExploredBuilding eb = m_LocationBuildings[i];
			if (eb.m_Building == building)
			{
				eb.m_Visited = true;
				return;
			}
		}
	}

	//! Сколько зданий текущей локации ещё не обойдено.
	int LocationUnvisitedCount()
	{
		int count;
		int i;
		for (i = 0; i < m_LocationBuildings.Count(); i++)
		{
			if (!m_LocationBuildings[i].m_Visited)
				count++;
		}
		return count;
	}

	//! Сколько зданий текущей локации уже обойдено.
	int LocationVisitedCount()
	{
		int count;
		int i;
		for (i = 0; i < m_LocationBuildings.Count(); i++)
		{
			if (m_LocationBuildings[i].m_Visited)
				count++;
		}
		return count;
	}

	//! Количество посещённых локаций (для дампа).
	int VisitedLocationCount()
	{
		return m_VisitedLocations.Count();
	}

	//! Текущая локация (null = вне локаций).
	dmWorldPoiLocation GetCurrentLocation()
	{
		return m_CurrentLocation;
	}

	//! (веха D) «делать больше нечего».
	bool IsNothingToDo()
	{
		return m_NothingToDo;
	}

	void SetNothingToDo(bool v)
	{
		m_NothingToDo = v;
	}

	//! (веха E) «переход в локацию».
	bool IsInTransit()
	{
		return m_InTransit;
	}

	void SetInTransit(bool v)
	{
		m_InTransit = v;
	}

	//! (веха E) Назначение кочёвки.
	dmWorldPoiLocation GetDestination()
	{
		return m_Destination;
	}

	void SetDestination(dmWorldPoiLocation loc)
	{
		m_Destination = loc;
	}

	void ClearDestination()
	{
		m_Destination = null;
	}

	//! (веха D) Накопить время в текущей локации.
	void TickLocationTime(float dt)
	{
		m_TimeInLocation = m_TimeInLocation + dt;
	}

	float GetTimeInLocation()
	{
		return m_TimeInLocation;
	}

	//! (веха E) Ближайшая НЕпосещённая локация (2D до позиции бота), или null.
	dmWorldPoiLocation GetNearestUnvisitedLocation(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return null;

		vector botPos = pawn.GetPosition();
		dmWorldPOIRegistry reg = dmWorldPOIRegistry.Get();
		dmWorldPoiLocation best = null;
		float bestSq = 0.0;
		vector locPos;
		float dSq;
		int i;
		for (i = 0; i < reg.LocationCount(); i++)
		{
			dmWorldPoiLocation loc = reg.GetLocation(i);
			if (!loc || HasVisited(loc.Id))
				continue;
			locPos = loc.Position;
			dSq = dmMath.DistSq2D(locPos, botPos);
			if (!best || dSq < bestSq)
			{
				best = loc;
				bestSq = dSq;
			}
		}
		return best;
	}
};
