//! dmBotPathfinder — minimal wrapper over the native navmesh API.
//!
//! The heavy lifting is the native AIWorld.FindPath (A* over the navmesh); here we
//! only hold the AIWorld reference and the two PGFilters used by the bot:
//!   - m_Filter       — walkable ground (WALK/DOOR/INSIDE) + vault/climb (JUMP/CLIMB),
//!     no swim/crawl/crouch/unreachable.
//!   - m_SampleFilter — snap a target onto the navmesh (everything except crawl/crouch).
//!
//! Deferred (see docs/plans/fsm-implementation-plan.md "Pathfinding"): ladders,
//! swimming, attachment navmesh, string-pulling, path-cost tuning.

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
		int include = PGPolyFlags.WALK | PGPolyFlags.DOOR | PGPolyFlags.INSIDE | PGPolyFlags.DISABLED | PGPolyFlags.JUMP | PGPolyFlags.CLIMB;
		int exclude = PGPolyFlags.SWIM | PGPolyFlags.SWIM_SEA | PGPolyFlags.CRAWL | PGPolyFlags.CROUCH | PGPolyFlags.UNREACHABLE;

		m_Filter = new PGFilter();
		m_Filter.SetFlags(include, exclude, PGPolyFlags.NONE);
		m_Filter.SetCost(PGAreaType.DOOR_CLOSED, 4.0);
		m_Filter.SetCost(PGAreaType.DOOR_OPENED, 10000.0);
		m_Filter.SetCost(PGAreaType.FENCE_WALL, 5.0);
		m_Filter.SetCost(PGAreaType.JUMP, 10.0);

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
		return m_AIWorld.FindPath(from, to, m_Filter, waypoints);
	}

	//! Snap a position to the nearest navmesh point within maxDist.
	bool SamplePosition(vector pos, float maxDist, out vector sampled)
	{
		if (!m_AIWorld)
			return false;

		return m_AIWorld.SampleNavmeshPosition(pos, maxDist, m_SampleFilter, sampled);
	}
}
