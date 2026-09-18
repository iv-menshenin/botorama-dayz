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

		//! Precompute degree and the deadend list once (O(nodes + edges)); node
		//! ids are contiguous 0..N-1 after SewNodes, so degree[]/farNode[] are
		//! indexed directly by node id — no scans inside the pair loop below.
		int nodeCount = graph.Nodes.Count();
		array<int> degree = new array<int>();
		int i;
		for (i = 0; i < nodeCount; i++)
			degree.Insert(0);

		for (i = 0; i < graph.Edges.Count(); i++)
		{
			degree[graph.Edges[i].From] = degree[graph.Edges[i].From] + 1;
			degree[graph.Edges[i].To] = degree[graph.Edges[i].To] + 1;
		}

		array<int> deadends = new array<int>();
		for (i = 0; i < nodeCount; i++)
		{
			if (degree[i] == 1)
				deadends.Insert(i);
		}

		//! For each deadend, its single far neighbour; -1 for non-deadends. The
		//! outward direction is then far.Pos - deadend.Pos, fetched in O(1).
		array<int> farNode = new array<int>();
		for (i = 0; i < nodeCount; i++)
			farNode.Insert(-1);
		for (i = 0; i < graph.Edges.Count(); i++)
		{
			if (degree[graph.Edges[i].From] == 1)
				farNode[graph.Edges[i].From] = graph.Edges[i].To;
			if (degree[graph.Edges[i].To] == 1)
				farNode[graph.Edges[i].To] = graph.Edges[i].From;
		}

		int ai;
		int bj;
		int ni;
		int nj;
		int farI;
		int farJ;
		float a1;
		float a2;
		vector dirI;
		vector dirJ;
		vector toJ;
		vector toI;
		dmRoadGap gap;
		for (ai = 0; ai < deadends.Count(); ai++)
		{
			ni = deadends[ai];
			for (bj = ai + 1; bj < deadends.Count(); bj++)
			{
				nj = deadends[bj];
				if (!WithinXZ(graph.Nodes[ni].Pos, graph.Nodes[nj].Pos, DM_ROAD_GAP_MAX))
					continue;
				farI = farNode[ni];
				farJ = farNode[nj];
				dirI = graph.Nodes[farI].Pos - graph.Nodes[ni].Pos;
				dirJ = graph.Nodes[farJ].Pos - graph.Nodes[nj].Pos;
				toJ = graph.Nodes[nj].Pos - graph.Nodes[ni].Pos;
				toI = graph.Nodes[ni].Pos - graph.Nodes[nj].Pos;
				a1 = dmRoadProbe.AngleDeg(dirI, toJ);
				a2 = dmRoadProbe.AngleDeg(dirJ, toI);
				if (a1 >= DM_ROAD_GAP_ANGLE)
					continue;
				if (a2 >= DM_ROAD_GAP_ANGLE)
					continue;

				gap = new dmRoadGap();
				gap.Id = list.Gaps.Count();
				gap.FromNode = graph.Nodes[ni].Id;
				gap.FromPos = graph.Nodes[ni].Pos;
				gap.ToNode = graph.Nodes[nj].Id;
				gap.ToPos = graph.Nodes[nj].Pos;
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

	//! True when two points are within dist in the XZ plane.
	private static bool WithinXZ(vector a, vector b, float dist)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz <= dist * dist;
	}
}
