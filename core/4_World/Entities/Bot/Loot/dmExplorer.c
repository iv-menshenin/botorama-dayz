//! dmExplorer — состояние мира (слой 1 лута): какие здания бот исследовал.
//!
//! Чистый домен: хранит здания (посещено/нет), сканирует окрестность раз в минуту
//! (DM_EXPLORE_TICK_INTERVAL), забывает далёкие (500 м любое / 200 м непосещённое).
//! GetNearest отдаёт ближайшее непосещённое здание. Никого не пишет, ссылок на
//! Wishlist/Requirements/dmNeeds нет.

//! Здание в памяти исследования: ссылка + флаг «посещено».
class dmExploredBuilding
{
	Building m_Building;
	bool m_Visited;
};

class dmExplorer
{
	ref array<ref dmExploredBuilding> m_Buildings;
	float m_TickAccum;   // троттлинг скана

	void dmExplorer()
	{
		m_Buildings = new array<ref dmExploredBuilding>();
		m_TickAccum = 0.0;
	}

	//! Тик из Update бота: раз в DM_EXPLORE_TICK_INTERVAL сканирует здания вокруг
	//! и забывает далёкие.
	void OnUpdate(dmAISurvivor bot, float pDt)
	{
		m_TickAccum += pDt;
		if (m_TickAccum < DM_EXPLORE_TICK_INTERVAL)
			return;
		m_TickAccum = 0.0;

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		vector botPos = pawn.GetPosition();

		//! Скан зданий в кубе DM_EXPLORE_SCAN_RADIUS вокруг бота.
		//! QueryFlags.STATIC возвращает статические сущности (здания) — эталон
		//! ExpansionWorld.GenerateRoamingLocations (SceneGetEntitiesInBox +
		//! QueryFlags.STATIC → IsBuilding). Физика тоже ловит статику, но scene-запрос
		//! со STATIC — подтверждённый путь для Building.
		array<EntityAI> entities = new array<EntityAI>();
		float r = DM_EXPLORE_SCAN_RADIUS;
		vector minPos = botPos - Vector(r, r, r);
		vector maxPos = botPos + Vector(r, r, r);
		DayZPlayerUtils.SceneGetEntitiesInBox(minPos, maxPos, entities, QueryFlags.STATIC);

		int i;
		for (i = 0; i < entities.Count(); i++)
		{
			Building building = Building.Cast(entities[i]);
			if (!building) continue;
			
			if (!FindBuilding(building))
			{
				dmExploredBuilding eb = new dmExploredBuilding();
				eb.m_Building = building;
				eb.m_Visited = false;
				m_Buildings.Insert(eb);
			}
		}

		ForgetFar(botPos);

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Explorer: scan found=" + entities.Count() + " tracked=" + m_Buildings.Count());
		#endif
	}

	//! Ближайшее НЕпосещённое здание в радиусе radius, или null.
	Building GetNearest(dmAISurvivor bot, float radius)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return null;

		vector botPos = pawn.GetPosition();
		Building best = null;
		float bestDist = 0.0;
		int i;
		for (i = 0; i < m_Buildings.Count(); i++)
		{
			dmExploredBuilding eb = m_Buildings[i];
			if (eb.m_Visited || !eb.m_Building)
				continue;
			vector bPos = eb.m_Building.GetPosition();
			vector d = bPos - botPos;
			d[1] = 0.0;
			float dist = d.Length();
			if (dist > radius)
				continue;
			if (!best || dist < bestDist)
			{
				best = eb.m_Building;
				bestDist = dist;
			}
		}
		return best;
	}

	//! Пометить здание посещённым.
	void MarkVisited(Building building)
	{
		dmExploredBuilding eb = FindBuilding(building);
		if (eb)
			eb.m_Visited = true;
	}

	//! Найти запись здания (или null).
	dmExploredBuilding FindBuilding(Building building)
	{
		int i;
		for (i = 0; i < m_Buildings.Count(); i++)
		{
			if (m_Buildings[i].m_Building == building)
				return m_Buildings[i];
		}
		return null;
	}

	//! Забыть: дальше DM_EXPLORE_FORGET_ANY (любое) или дальше
	//! DM_EXPLORE_FORGET_UNVISITED (непосещённое). С обратным циклом.
	void ForgetFar(vector botPos)
	{
		int i;
		for (i = m_Buildings.Count() - 1; i >= 0; i--)
		{
			dmExploredBuilding eb = m_Buildings[i];
			if (!eb.m_Building)
			{
				m_Buildings.Remove(i);
				continue;
			}
			vector bPos = eb.m_Building.GetPosition();
			vector d = bPos - botPos;
			d[1] = 0.0;
			float dist = d.Length();
			if (dist > DM_EXPLORE_FORGET_ANY)
				m_Buildings.Remove(i);
			else if (!eb.m_Visited && dist > DM_EXPLORE_FORGET_UNVISITED)
				m_Buildings.Remove(i);
		}
	}
};
