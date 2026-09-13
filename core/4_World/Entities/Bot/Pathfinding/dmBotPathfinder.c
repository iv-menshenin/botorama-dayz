//! dmBotPathfinder — minimal wrapper over the native navmesh API.
//!
//! The heavy lifting is the native AIWorld.FindPath (A* over the navmesh); here we
//! only hold the AIWorld reference and the two PGFilters used by the bot:
//!   - m_Filter       — walkable ground (WALK/DOOR/INSIDE) + vault/climb (JUMP/CLIMB)
//!     + ladders (LADDER, cheap — routes between navmesh floors), no swim/crawl/
//!     crouch/unreachable.
//!   - m_SampleFilter — snap a target onto the navmesh (everything except crawl/crouch).
//!
//! Deferred (see docs/plans/fsm-implementation-plan.md "Pathfinding"): swimming,
//! attachment navmesh, string-pulling, path-cost tuning.

//! Один сегмент маршрута: navmesh-путь ИЛИ подъём/спуск по лестнице.
class dmBotRouteSegment
{
	bool m_IsLadder = false;            // true = лестница, false = navmesh-путь
	ref array<vector> m_Waypoints;      // waypoints (для navmesh-сегмента)
	Building m_Building;                // для лестничного сегмента
	ref dmBotLadder m_Ladder;
	int m_Direction = 1;                // +1 вверх, -1 вниз
}

class dmBotPathfinder
{
	AIWorld m_AIWorld;
	ref PGFilter m_Filter;
	ref PGFilter m_SampleFilter;

	void dmBotPathfinder()
	{
		m_AIWorld = GetGame().GetWorld().GetAIWorld();

		//! DISABLED (=closed door) is included so A* routes THROUGH closed doors;
		//! DOOR_CLOSED is cheap (the bot opens it) while DOOR_OPENED is expensive
		//! (an open door is a physical obstacle to walk around, not through).
		//! JUMP/CLIMB route through vault/climb obstacles (fences, low walls); they
		//! exclude CRAWL/CROUCH so A* doesn't route through crawl-only polygons.
		//! LADDER is included so A* can cross between navmesh floors via a ladder;
		//! its cost is cheap (1.0) so the bot walks toward the ladder.
		int include = PGPolyFlags.WALK | PGPolyFlags.DOOR | PGPolyFlags.INSIDE | PGPolyFlags.DISABLED | PGPolyFlags.JUMP | PGPolyFlags.CLIMB | PGPolyFlags.LADDER;
		int exclude = PGPolyFlags.SWIM | PGPolyFlags.SWIM_SEA | PGPolyFlags.CRAWL | PGPolyFlags.CROUCH | PGPolyFlags.UNREACHABLE;

		m_Filter = new PGFilter();
		m_Filter.SetFlags(include, exclude, PGPolyFlags.NONE);
		m_Filter.SetCost(PGAreaType.DOOR_CLOSED, 4.0);
		m_Filter.SetCost(PGAreaType.DOOR_OPENED, 10000.0);
		m_Filter.SetCost(PGAreaType.FENCE_WALL, 5.0);
		m_Filter.SetCost(PGAreaType.JUMP, 10.0);
		m_Filter.SetCost(PGAreaType.LADDER, 1.0);

		m_SampleFilter = new PGFilter();
		m_SampleFilter.SetFlags(PGPolyFlags.ALL & ~(PGPolyFlags.CRAWL | PGPolyFlags.CROUCH), PGPolyFlags.CRAWL | PGPolyFlags.CROUCH, PGPolyFlags.NONE);
	}

	//! A* path from `from` to `to`. Fills `waypoints` (cleared first); returns false
	//! when no path exists (or the AIWorld is unavailable).
	bool FindPath(vector from, vector to, inout array<vector> waypoints)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Path.Find");
		#endif

		if (!m_AIWorld)
			return false;

		waypoints.Clear();
		bool found = m_AIWorld.FindPath(from, to, m_Filter, waypoints);

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] FindPath from=" + from + " to=" + to + " found=" + found);
		dmBotLog.Debug("[PATH] FindPath n=" + waypoints.Count());
		int wi;
		for (wi = 0; wi < waypoints.Count(); wi++)
			dmBotLog.Debug("[PATH] FindPath wp[" + wi + "]=" + waypoints[wi]);
		#endif
		return found;
	}

	//! Snap a position to the nearest navmesh point within maxDist.
	bool SamplePosition(vector pos, float maxDist, out vector sampled)
	{
		if (!m_AIWorld)
			return false;

		bool ok = m_AIWorld.SampleNavmeshPosition(pos, maxDist, m_SampleFilter, sampled);
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] SamplePosition pos=" + pos + " maxDist=" + maxDist + " ok=" + ok);
		dmBotLog.Debug("[PATH] SamplePosition sampled=" + sampled);
		#endif
		return ok;
	}

	//! Ladder-aware маршрут из сегментов. Заполняет `segments` (очищает); false — нет маршрута.
	//! `ignoreObj` — объект, который raycast поиска здания пропускает (цель сопровождения).
	bool FindRoute(vector from, vector to, Object ignoreObj, inout array<ref dmBotRouteSegment> segments)
	{
		segments.Clear();
		ref map<string, bool> visited = new map<string, bool>();
		return FindRouteRecursive(from, to, ignoreObj, visited, segments, 0);
	}

	private bool FindRouteRecursive(vector from, vector to, Object ignoreObj, map<string, bool> visited, inout array<ref dmBotRouteSegment> outSegments, int depth)
	{
		if (depth > DM_NAV_MAX_DEPTH)
			return false;

		vector sampledTo;
		if (!SamplePosition(to, DM_PATH_SAMPLE_RADIUS, sampledTo))
			return false;

		ref array<vector> path = new array<vector>();
		if (!m_AIWorld.FindPath(from, sampledTo, m_Filter, path) || path.Count() == 0)
			return false;

		vector last = path[path.Count() - 1];
		if (Math.AbsFloat(last[1] - sampledTo[1]) <= DM_NAV_GAP)
		{
			dmBotRouteSegment seg = new dmBotRouteSegment();
			seg.m_IsLadder = false;
			seg.m_Waypoints = path;
			outSegments.Insert(seg);
			return true;
		}

		int dir = 1;
		if (sampledTo[1] < last[1])
			dir = -1;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] FindRoute gap dir=" + dir + " lastY=" + last[1] + " toY=" + sampledTo[1]);
		#endif

		Building building = FindLadderBuildingAt(sampledTo, ignoreObj);
		#ifdef DM_BOT_DEBUG_PATHFINDER
		if (building)
			dmBotLog.Debug("[PATH] FindRoute building=" + building.GetType());
		else
			dmBotLog.Debug("[PATH] FindRoute building=NONE");
		#endif
		if (!building)
			return false;

		ref array<ref dmBotLadder> ladders = dmBotLadderCache.GetInstance().GetLadders(building);
		if (!ladders || ladders.Count() == 0)
			return false;

		int i;
		for (i = 0; i < ladders.Count(); i++)
		{
			dmBotLadder ladder = ladders[i];

			string key = building.GetType() + "_" + ladder.m_Index;
			bool seen = false;
			if (visited.Find(key, seen))
				continue;

			vector nearModel = ladder.m_Bottom;
			vector farModel = ladder.m_Top;
			if (dir < 0)
			{
				nearModel = ladder.m_Top;
				farModel = ladder.m_Bottom;
			}
			vector near = building.ModelToWorld(nearModel);
			vector far = building.ModelToWorld(farModel);

			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] FindRoute try ladder i=" + ladder.m_Index + " near=" + near + " far=" + far);
			#endif

			visited.Set(key, true);

			ref array<ref dmBotRouteSegment> sub = new array<ref dmBotRouteSegment>();
			if (FindRouteRecursive(far, to, ignoreObj, visited, sub, depth + 1))
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] FindRoute recurse far->to OK");
				#endif
				ref array<vector> pathToNear = new array<vector>();
				if (m_AIWorld.FindPath(from, near, m_Filter, pathToNear) && pathToNear.Count() > 0)
				{
					dmBotRouteSegment segNav = new dmBotRouteSegment();
					segNav.m_IsLadder = false;
					segNav.m_Waypoints = pathToNear;
					outSegments.Insert(segNav);

					dmBotRouteSegment segLadder = new dmBotRouteSegment();
					segLadder.m_IsLadder = true;
					segLadder.m_Building = building;
					segLadder.m_Ladder = ladder;
					segLadder.m_Direction = dir;
					outSegments.Insert(segLadder);

					int j;
					for (j = 0; j < sub.Count(); j++)
						outSegments.Insert(sub[j]);

					return true;
				}
			}

			visited.Remove(key);
		}

		return false;
	}

	//! Здание с лестницей под/над точкой: raycast ВНИЗ (фолбэк ВВЕРХ).
	private Building FindLadderBuildingAt(vector pos, Object ignoreObj)
	{
		Building building;
		RaycastRVParams rp = new RaycastRVParams(pos + Vector(0.0, 1.0, 0.0), pos + Vector(0.0, -50.0, 0.0), ignoreObj);
		rp.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(rp, hits) && hits.Count() > 0)
		{
			building = Building.Cast(hits[0].obj);
			if (building)
				return building;
		}

		RaycastRVParams rpUp = new RaycastRVParams(pos + Vector(0.0, 1.0, 0.0), pos + Vector(0.0, 25.0, 0.0), ignoreObj);
		rpUp.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> hitsUp = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(rpUp, hitsUp) && hitsUp.Count() > 0)
		{
			building = Building.Cast(hitsUp[0].obj);
			if (building)
				return building;
		}
		return null;
	}
}
