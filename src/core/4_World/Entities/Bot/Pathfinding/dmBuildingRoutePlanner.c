//! dmBuildingRoutePlanner — жадный «коммивояжёр» интерьер-точек здания с
//! ограниченной разведкой (DM_EXPLORE_ROUTE_PROBE_COUNT кандидатов) и метрикой
//! «сегменты пути + 2D-длина» (тайбрейкер). Недостижимая точка — бесконечные
//! сегменты, чтобы SELECT её не выбрал, пока есть альтернатива.
//!
//! Планировщик НЕ знает, зачем ему маршрут — только «упорядочь точки от старта».
//! Машина состояний: PENDING (выбрать евклид-ближайшие кандидаты) → CALC (снап +
//! одна A* за тик на кандидата) → SELECT (перенести лучшую точку в готовый маршрут)
//! → назад в PENDING; когда очередь пуста — DONE. Один шаг за Tick(), чтобы серия
//! A* не блокировала серверный кадр на крупном здании. m_Current всегда держит
//! снапнутую на навмеш точку.

enum dmRoutePlannerState
{
	PENDING,
	CALC,
	SELECT,
	DONE
}

//! Кандидат разведки: авторская точка + её снап на навмеш + метрика пути.
class dmProbe
{
	vector m_Point;     // исходная авторская точка (цель MoveTo)
	vector m_Snapped;   // снапнутая на навмеш (новый m_Current планировщика)
	int m_Segments;     // сегменты пути от m_Current (∞ = недостижимая)
	float m_Length;     // 2D-длина пути (тайбрейкер)
}

class dmBuildingRoutePlanner
{
	//! Сегменты недостижимой точки (SELECT не выберет её, пока есть достижимая).
	static const int DM_ROUTE_SEGMENTS_INFINITE = 1000000;

	private ref array<vector> m_Queue;    // оставшиеся точки (очередь)
	private ref array<vector> m_Ready;    // готовый маршрут (упорядочен)
	vector m_Current;                      // текущая точка (снапнутая на навмеш)
	ref array<ref dmProbe> m_Probes;       // кандидаты разведки (ближайшие)
	int m_ProbeScanIdx;                    // следующий кандидат на разведку
	dmRoutePlannerState m_State;           // состояние машины
	ref dmBotPathfinder m_Pathfinder;      // A* (один на планировщик)

	void dmBuildingRoutePlanner()
	{
		m_Queue = new array<vector>();
		m_Ready = new array<vector>();
		m_Probes = new array<ref dmProbe>();
		m_ProbeScanIdx = 0;
		m_State = dmRoutePlannerState.PENDING;
		m_Pathfinder = new dmBotPathfinder();
	}

	void Init(vector start, array<vector> points)
	{
		m_Current = start;
		vector sampledStart;
		if (m_Pathfinder.SamplePosition(start, DM_PATH_SAMPLE_RADIUS, sampledStart))
			m_Current = sampledStart;

		m_Queue.Clear();
		int i;
		for (i = 0; i < points.Count(); i++)
			m_Queue.Insert(points[i]);
		m_Ready.Clear();
		m_Probes.Clear();
		m_ProbeScanIdx = 0;
		m_State = dmRoutePlannerState.PENDING;
	}

	void Tick()
	{
		switch (m_State)
		{
		case dmRoutePlannerState.PENDING:
			TickPending();
			break;
		case dmRoutePlannerState.CALC:
			TickCalc();
			break;
		case dmRoutePlannerState.SELECT:
			TickSelect();
			break;
		case dmRoutePlannerState.DONE:
			break;
		}
	}

	//! Следующая точка готового маршрута (и удаление её из m_Ready). false — пусто.
	bool NextPoint(out vector p)
	{
		if (m_Ready.Count() == 0)
		{
			p = vector.Zero;
			return false;
		}
		p = m_Ready[0];
		m_Ready.Remove(0);
		return true;
	}

	//! Удалить точку из очереди (и из готового маршрута, если уже там). Для скипа
	//! пройденной точки. Сравнение по значению (точки — из одного массива).
	void RemovePoint(vector p)
	{
		RemoveFromQueue(p);
		int i;
		for (i = 0; i < m_Ready.Count(); i++)
		{
			if (m_Ready[i] == p)
			{
				m_Ready.Remove(i);
				return;
			}
		}
	}

	//! Удалить точки очереди и готового маршрута в радиусе `radius` от `pos` (3D).
	void RemovePointsNear(vector pos, float radius)
	{
		float radiusSq = radius * radius;
		ref array<vector> toRemove = new array<vector>();
		int i;
		for (i = 0; i < m_Queue.Count(); i++)
		{
			if (vector.DistanceSq(m_Queue[i], pos) <= radiusSq)
				toRemove.Insert(m_Queue[i]);
		}
		for (i = 0; i < m_Ready.Count(); i++)
		{
			if (vector.DistanceSq(m_Ready[i], pos) <= radiusSq)
				toRemove.Insert(m_Ready[i]);
		}
		for (i = 0; i < toRemove.Count(); i++)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Explore] route скипнута (близко) " + toRemove[i]);
			#endif
			RemovePoint(toRemove[i]);
		}
	}

	bool IsDone()
	{
		if (m_State != dmRoutePlannerState.DONE)
			return false;
		return m_Queue.Count() == 0 && m_Ready.Count() == 0;
	}

	private void TickPending()
	{
		if (m_Queue.Count() == 0)
		{
			m_State = dmRoutePlannerState.DONE;
			return;
		}

		SelectNearestProbes();
		m_ProbeScanIdx = 0;
		m_State = dmRoutePlannerState.CALC;
	}

	private void TickCalc()
	{
		if (m_ProbeScanIdx >= m_Probes.Count())
		{
			m_State = dmRoutePlannerState.SELECT;
			return;
		}

		dmProbe probe = m_Probes[m_ProbeScanIdx];

		vector sampledTo;
		if (!m_Pathfinder.SamplePosition(probe.m_Point, DM_PATH_SAMPLE_RADIUS, sampledTo))
		{
			probe.m_Segments = DM_ROUTE_SEGMENTS_INFINITE;
			probe.m_Length = 0.0;
			m_ProbeScanIdx = m_ProbeScanIdx + 1;
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Explore] route кандидат " + probe.m_Point + " недостижимая (нет снапа)");
			#endif
			return;
		}

		probe.m_Snapped = sampledTo;
		ref array<vector> waypoints = new array<vector>();
		if (!m_Pathfinder.FindPath(m_Current, sampledTo, waypoints))
		{
			probe.m_Segments = DM_ROUTE_SEGMENTS_INFINITE;
			probe.m_Length = 0.0;
			m_ProbeScanIdx = m_ProbeScanIdx + 1;
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Explore] route кандидат " + probe.m_Point + " недостижимая");
			#endif
			return;
		}

		probe.m_Segments = waypoints.Count() - 1;
		probe.m_Length = m_Pathfinder.PathLength2D(waypoints);

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Explore] route кандидат " + probe.m_Point + " сегментов=" + probe.m_Segments + " длина=" + probe.m_Length);
		#endif

		m_ProbeScanIdx = m_ProbeScanIdx + 1;
	}

	private void TickSelect()
	{
		int i;
		int bestIdx = -1;
		int bestSegments = 0;
		float bestLength = 0.0;
		bool hasReachable = false;
		for (i = 0; i < m_Probes.Count(); i++)
		{
			dmProbe probe = m_Probes[i];
			if (probe.m_Segments >= DM_ROUTE_SEGMENTS_INFINITE)
				continue;
			hasReachable = true;
			if (bestIdx < 0 || probe.m_Segments < bestSegments || (probe.m_Segments == bestSegments && probe.m_Length < bestLength))
			{
				bestIdx = i;
				bestSegments = probe.m_Segments;
				bestLength = probe.m_Length;
			}
		}

		if (!hasReachable)
		{
			for (i = 0; i < m_Probes.Count(); i++)
				RemoveFromQueue(m_Probes[i].m_Point);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Explore] route все кандидаты недостижимы, пропускаю " + m_Probes.Count() + " точек");
			#endif
			m_State = dmRoutePlannerState.PENDING;
			return;
		}

		dmProbe best = m_Probes[bestIdx];
		RemoveFromQueue(best.m_Point);
		m_Ready.Insert(best.m_Point);
		m_Current = best.m_Snapped;

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Explore] route выбрана " + best.m_Point + " сегментов=" + bestSegments + " длина=" + bestLength);
		#endif

		m_State = dmRoutePlannerState.PENDING;
	}

	//! Евклид-ближайшие (2D к m_Current) точки очереди в m_Probes.
	private void SelectNearestProbes()
	{
		m_Probes.Clear();

		int probeCount = DM_EXPLORE_ROUTE_PROBE_COUNT;
		if (m_Queue.Count() < probeCount)
			probeCount = m_Queue.Count();

		ref array<bool> chosen = new array<bool>();
		int i;
		int pick;
		int j;
		int bestIdx;
		float dSq;
		float bestSq;

		for (i = 0; i < m_Queue.Count(); i++)
			chosen.Insert(false);

		for (pick = 0; pick < probeCount; pick++)
		{
			bestIdx = -1;
			bestSq = 0.0;
			for (j = 0; j < m_Queue.Count(); j++)
			{
				if (chosen[j])
					continue;
				dSq = dmMath.DistSq2D(m_Queue[j], m_Current);
				if (bestIdx < 0 || dSq < bestSq)
				{
					bestIdx = j;
					bestSq = dSq;
				}
			}
			dmProbe probe = new dmProbe();
			probe.m_Point = m_Queue[bestIdx];
			m_Probes.Insert(probe);
			chosen[bestIdx] = true;
		}
	}

	//! Удалить точку из очереди (по значению).
	private void RemoveFromQueue(vector p)
	{
		int i;
		for (i = 0; i < m_Queue.Count(); i++)
		{
			if (m_Queue[i] == p)
			{
				m_Queue.Remove(i);
				return;
			}
		}
	}
}
