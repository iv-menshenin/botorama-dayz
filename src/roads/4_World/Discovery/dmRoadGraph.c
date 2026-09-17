//! dmRoadGraph — road network graph (iteration 3).
//!
//! The discovered road network is a JSON-serializable graph: vertices (seeds,
//! junctions, dead ends) connected by edges (road polylines). dmRoadGraphBuilder
//! floods the network from a set of seeds using dmRoadProbe.FindBranches to find
//! the road directions and dmRoadProbe.WalkDirBranch to walk each branch, with
//! cell-grid dedup (visited), node-cell dedup (vertex reuse) and junction
//! detection. See docs/plans/road-discovery.md for the overall design.

//! A graph vertex: a seed, a junction or a dead end.
class dmRoadGraphNode
{
	int Id;
	vector Pos;
	string Kind;
}

//! A graph edge: a road polyline between two vertices.
class dmRoadGraphEdge
{
	int Id;
	int From;
	int To;
	ref array<vector> Points;
	float Length;
	int Surface;
}

//! The discovered road graph (root of road_graph.json).
class dmRoadGraph
{
	ref array<ref dmRoadGraphNode> Nodes;
	ref array<ref dmRoadGraphEdge> Edges;
}

//! One flood-fill queue item: a point to expand, the direction travelled to
//! reach it, and the id of the vertex we came from.
class dmRoadGraphQueueItem
{
	vector Point;
	vector CameFrom;
	int FromNode;
}

//! Flood-fill road graph builder.
class dmRoadGraphBuilder
{
	static dmRoadGraph Build(array<vector> seeds)
	{
		dmRoadGraphBuilder builder = new dmRoadGraphBuilder();
		return builder.BuildGraph(seeds);
	}

	private ref dmRoadGraph m_Graph;
	private ref map<string, bool> m_Visited;
	private ref map<string, int> m_NodeCells;
	private ref array<ref dmRoadGraphQueueItem> m_Queue;
	private int m_QueueHead;
	private int m_NextNodeId;
	private int m_NextEdgeId;

	void dmRoadGraphBuilder()
	{
		m_Graph = null;
		m_Visited = new map<string, bool>();
		m_NodeCells = new map<string, int>();
		m_Queue = new array<ref dmRoadGraphQueueItem>();
		m_QueueHead = 0;
		m_NextNodeId = 0;
		m_NextEdgeId = 0;
	}

	//! Flood-fill the road network from the seeds and return the graph.
	private dmRoadGraph BuildGraph(array<vector> seeds)
	{
		m_Graph = new dmRoadGraph();
		m_Graph.Nodes = new array<ref dmRoadGraphNode>();
		m_Graph.Edges = new array<ref dmRoadGraphEdge>();

		int i;
		for (i = 0; i < seeds.Count(); i++)
		{
			vector seed = seeds[i];
			if (!dmRoadSensor.IsDrivable(seed[0], seed[2]))
			{
				dmBotLog.Error("[ROADG] seed not drivable (" + seed[0] + "," + seed[2] + ")");
				continue;
			}
			int nodeId = AddNode(seed, "seed");
			Enqueue(seed, vector.Zero, nodeId);
			m_Visited.Set(dmRoadProbe.CellKey(seed[0], seed[2], DM_ROAD_DEDUP_CELL), true);
		}

		while (m_QueueHead < m_Queue.Count())
		{
			Expand(m_Queue[m_QueueHead]);
			m_QueueHead = m_QueueHead + 1;
		}

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROADG] nodes=" + m_Graph.Nodes.Count() + " edges=" + m_Graph.Edges.Count());
		#endif

		return m_Graph;
	}

	//! Expand one queue item: walk every non-backward branch from its point.
	private void Expand(dmRoadGraphQueueItem item)
	{
		vector p = item.Point;
		vector cameFrom = item.CameFrom;
		int fromNode = item.FromNode;

		array<vector> dirs = dmRoadProbe.FindBranches(p);
		int i;
		for (i = 0; i < dirs.Count(); i++)
		{
			vector dir = dirs[i];

			if (cameFrom != vector.Zero)
			{
				vector back = Vector(-cameFrom[0], 0.0, -cameFrom[2]);
				if (dmRoadProbe.AngleDeg(dir, back) < DM_ROAD_BACK_ANGLE)
					continue;
			}

			dmRoadBranch walk = dmRoadProbe.WalkDirBranch(p, dir, cameFrom, m_Visited);
			if (walk.Steps < DM_ROAD_MIN_BRANCH_STEPS)
				continue;

			HandleBranch(p, dir, fromNode, walk);
		}
	}

	//! Turn a finished branch into an edge (and possibly a vertex), then recurse
	//! from a junction. Visited branches (road already covered) add nothing.
	private void HandleBranch(vector seed, vector dir, int fromNode, dmRoadBranch walk)
	{
		if (walk.Status == "visited")
			return;

		int endNode = fromNode;
		vector endPos = seed;

		if (walk.Status != "loop")
		{
			endPos = walk.Points[walk.Points.Count() - 1];
			if (walk.Status == "junction")
				endNode = FindOrCreateNode(endPos, "junction");
			else
				endNode = FindOrCreateNode(endPos, "deadend");
		}

		AddEdge(fromNode, endNode, walk);
		MarkWalkCells(walk);

		if (walk.Status == "junction")
			Enqueue(endPos, dir, endNode);
	}

	//! Create a new vertex and register its node-cell.
	private int AddNode(vector pos, string kind)
	{
		if (pos[1] == 0.0)
			pos[1] = GetGame().SurfaceY(pos[0], pos[2]);

		dmRoadGraphNode node = new dmRoadGraphNode();
		node.Id = m_NextNodeId;
		node.Pos = pos;
		node.Kind = kind;
		m_Graph.Nodes.Insert(node);
		m_NodeCells.Set(dmRoadProbe.CellKey(pos[0], pos[2], DM_ROAD_NODE_CELL), node.Id);
		m_NextNodeId = m_NextNodeId + 1;
		return node.Id;
	}

	//! Reuse an existing vertex in the same node-cell, else create a new one.
	private int FindOrCreateNode(vector pos, string kind)
	{
		int existing;
		if (m_NodeCells.Find(dmRoadProbe.CellKey(pos[0], pos[2], DM_ROAD_NODE_CELL), existing))
			return existing;
		return AddNode(pos, kind);
	}

	//! Create an edge from a walked branch (polyline, length, dominant surface).
	private void AddEdge(int fromNode, int toNode, dmRoadBranch walk)
	{
		dmRoadGraphEdge edge = new dmRoadGraphEdge();
		edge.Id = m_NextEdgeId;
		edge.From = fromNode;
		edge.To = toNode;
		edge.Points = new array<vector>();
		int i;
		for (i = 0; i < walk.Points.Count(); i++)
			edge.Points.Insert(walk.Points[i]);
		edge.Length = PolylineLength(walk.Points);
		edge.Surface = DominantSurface(walk.Surfaces);
		m_Graph.Edges.Insert(edge);
		m_NextEdgeId = m_NextEdgeId + 1;
	}

	//! Mark the interior cells of a walked polyline (not the endpoints, which are
	//! vertices) as visited so other branches dedup against them.
	private void MarkWalkCells(dmRoadBranch walk)
	{
		int last = walk.Points.Count() - 1;
		int i;
		for (i = 1; i < last; i++)
			m_Visited.Set(dmRoadProbe.CellKey(walk.Points[i][0], walk.Points[i][2], DM_ROAD_DEDUP_CELL), true);
	}

	//! Total 3D length of a polyline.
	private float PolylineLength(array<vector> points)
	{
		float length = 0.0;
		int i;
		for (i = 1; i < points.Count(); i++)
			length = length + vector.Distance(points[i - 1], points[i]);
		return length;
	}

	//! Most frequent surface class across the branch's points.
	private int DominantSurface(array<int> surfaces)
	{
		if (!surfaces || surfaces.Count() == 0)
			return dmRoadSurfaceClass.ROAD_UNKNOWN;

		int best = surfaces[0];
		int bestCount = 0;
		int i;
		int j;
		int count;
		for (i = 0; i < surfaces.Count(); i++)
		{
			count = 0;
			for (j = 0; j < surfaces.Count(); j++)
			{
				if (surfaces[j] == surfaces[i])
					count = count + 1;
			}
			if (count > bestCount)
			{
				bestCount = count;
				best = surfaces[i];
			}
		}
		return best;
	}

	//! Push a point onto the flood-fill queue.
	private void Enqueue(vector point, vector cameFrom, int fromNode)
	{
		dmRoadGraphQueueItem item = new dmRoadGraphQueueItem();
		item.Point = point;
		item.CameFrom = cameFrom;
		item.FromNode = fromNode;
		m_Queue.Insert(item);
	}
}
