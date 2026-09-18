//! dmRoadGap — road gap detection: pairs of degree-1 vertices (deadends) that
//! point at each other across a missing stretch of road. A gap is saved as a
//! pending "road_gaps.json" record for later human/planner resolution.

//! A road gap: two deadends that point at each other across a missing stretch.
class dmRoadGap
{
	int Id;
	int FromNode; vector FromPos;
	int ToNode;   vector ToPos;
	float Distance;    // XZ distance between the deadends, m
	float AngleA;      // angle (deg) of From's outward direction vs (To-From)
	float AngleB;      // angle (deg) of To's outward direction vs (From-To)
	string Status;     // "pending" | "resolved" | "refused"
}

//! Root of road_gaps.json.
class dmRoadGapList
{
	int Version;
	string WorldName;
	ref array<ref dmRoadGap> Gaps;
}

//! Detect gaps on a built graph: pairs of degree-1 vertices (deadends) that aim
//! at each other within DM_ROAD_GAP_MAX and under DM_ROAD_GAP_ANGLE.
class dmRoadGapDetector
{
	static dmRoadGapList Detect(dmRoadGraph graph)
	{
		dmRoadGapList list = new dmRoadGapList();
		list.Version = 1;
		list.WorldName = GetGame().GetWorldName();
		list.Gaps = new array<ref dmRoadGap>();

		int i;
		int j;
		float a1;
		float a2;
		dmRoadGap gap;
		for (i = 0; i < graph.Nodes.Count(); i++)
		{
			if (Degree(graph, graph.Nodes[i].Id) != 1)
				continue;
			for (j = i + 1; j < graph.Nodes.Count(); j++)
			{
				if (Degree(graph, graph.Nodes[j].Id) != 1)
					continue;
				if (!WithinXZ(graph.Nodes[i].Pos, graph.Nodes[j].Pos, DM_ROAD_GAP_MAX))
					continue;
				if (!AimAngles(graph, i, j, a1, a2))
					continue;

				gap = new dmRoadGap();
				gap.Id = list.Gaps.Count();
				gap.FromNode = graph.Nodes[i].Id;
				gap.FromPos = graph.Nodes[i].Pos;
				gap.ToNode = graph.Nodes[j].Id;
				gap.ToPos = graph.Nodes[j].Pos;
				gap.Distance = vector.Distance(gap.FromPos, gap.ToPos);
				gap.AngleA = a1;
				gap.AngleB = a2;
				gap.Status = "pending";
				list.Gaps.Insert(gap);
			}
		}

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROADGAP] gaps=" + list.Gaps.Count());
		#endif

		return list;
	}

	//! Number of edges incident to a vertex.
	private static int Degree(dmRoadGraph graph, int id)
	{
		int deg = 0;
		int i;
		for (i = 0; i < graph.Edges.Count(); i++)
		{
			if (graph.Edges[i].From == id || graph.Edges[i].To == id)
				deg = deg + 1;
		}
		return deg;
	}

	//! Position of a vertex by id (scan).
	private static vector NodePos(dmRoadGraph graph, int id)
	{
		int i;
		for (i = 0; i < graph.Nodes.Count(); i++)
		{
			if (graph.Nodes[i].Id == id)
				return graph.Nodes[i].Pos;
		}
		return vector.Zero;
	}

	//! Outward direction (XZ) from a deadend vertex toward the far end of its
	//! single incident edge. Returns false when the vertex has no incident edge.
	private static bool Outward(dmRoadGraph graph, int id, out vector dir)
	{
		int i;
		int farId;
		vector farPos;
		vector pos;
		for (i = 0; i < graph.Edges.Count(); i++)
		{
			farId = -1;
			if (graph.Edges[i].From == id)
				farId = graph.Edges[i].To;
			else if (graph.Edges[i].To == id)
				farId = graph.Edges[i].From;
			if (farId < 0)
				continue;
			farPos = NodePos(graph, farId);
			pos = NodePos(graph, id);
			dir = farPos - pos;
			return true;
		}
		return false;
	}

	//! True when two points are within dist in the XZ plane.
	private static bool WithinXZ(vector a, vector b, float dist)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz <= dist * dist;
	}

	//! True when two deadends point at each other across a gap (each outward
	//! direction points toward the other vertex), filling a1/a2 with the two
	//! aim angles (degrees).
	private static bool AimAngles(dmRoadGraph graph, int i, int j, out float a1, out float a2)
	{
		vector dirI;
		vector dirJ;
		vector toJ;
		vector toI;
		if (!Outward(graph, graph.Nodes[i].Id, dirI))
			return false;
		if (!Outward(graph, graph.Nodes[j].Id, dirJ))
			return false;
		toJ = graph.Nodes[j].Pos - graph.Nodes[i].Pos;
		toI = graph.Nodes[i].Pos - graph.Nodes[j].Pos;
		a1 = dmRoadProbe.AngleDeg(dirI, toJ);
		a2 = dmRoadProbe.AngleDeg(dirJ, toI);
		if (a1 >= DM_ROAD_GAP_ANGLE)
			return false;
		if (a2 >= DM_ROAD_GAP_ANGLE)
			return false;
		return true;
	}
}
