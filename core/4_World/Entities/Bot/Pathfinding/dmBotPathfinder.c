//! dmBotPathfinder — minimal wrapper over the native navmesh API.
//!
//! The heavy lifting is the native AIWorld.FindPath (A* over the navmesh); here we
//! only hold the AIWorld reference and the two PGFilters used by the bot:
//!   - m_Filter       — walkable ground (WALK/DOOR/INSIDE), no swim/special/unreachable.
//!   - m_SampleFilter — snap a target onto the navmesh (everything except crawl/crouch).
//!
//! Deferred (see Reference/fsm-implementation-plan.md "Pathfinding"): door handling,
//! vault/climb, ladders, swimming, attachment navmesh, string-pulling, path-cost tuning.

class dmBotPathfinder
{
	AIWorld m_AIWorld;
	ref PGFilter m_Filter;
	ref PGFilter m_SampleFilter;

	void dmBotPathfinder()
	{
		m_AIWorld = GetGame().GetWorld().GetAIWorld();

		int include = PGPolyFlags.WALK | PGPolyFlags.DOOR | PGPolyFlags.INSIDE;
		int exclude = PGPolyFlags.SWIM | PGPolyFlags.SWIM_SEA | PGPolyFlags.SPECIAL | PGPolyFlags.UNREACHABLE;

		m_Filter = new PGFilter();
		m_Filter.SetFlags(include, exclude, PGPolyFlags.NONE);

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
