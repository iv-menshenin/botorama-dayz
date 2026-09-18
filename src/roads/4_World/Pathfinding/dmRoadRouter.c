//! dmRoadRouter — coarse-to-fine road route planner (settlement/junction skeleton).
//!
//! Snap a start/target onto the road graphs (nearest node by XZ distance), run a
//! cheap O(V^2) Dijkstra over the coarse skeleton (settlements + lone junctions),
//! then stream the exact route in chunks: coarse legs expand to the full-graph
//! polylines (Edge.Path provenance), settlement interiors are joined by a local
//! A* over the full graph, and the off-road start/target are joined to the first/
//! last leg boundary the same way. Chunks are emitted incrementally via NextChunk;
//! position duplicates at seams are dropped so the concatenation is continuous.

//! Binary min-heap over (node, f-score) pairs stored as two parallel arrays.
//! Used by dmRoadRouter.FindFullPath (A* on the full graph).
class dmRouterMinHeap
{
	private ref array<int> m_Node;
	private ref array<float> m_Key;

	void dmRouterMinHeap()
	{
		m_Node = new array<int>();
		m_Key = new array<float>();
	}

	void Push(int node, float key)
	{
		m_Node.Insert(node);
		m_Key.Insert(key);
		SiftUp(m_Node.Count() - 1);
	}

	bool Pop(out int node, out float key)
	{
		int last;
		if (m_Node.Count() == 0)
		{
			node = -1;
			key = 0.0;
			return false;
		}
		node = m_Node[0];
		key = m_Key[0];
		last = m_Node.Count() - 1;
		m_Node[0] = m_Node[last];
		m_Key[0] = m_Key[last];
		m_Node.Remove(last);
		m_Key.Remove(last);
		if (m_Node.Count() > 0)
			SiftDown(0);
		return true;
	}

	int Count()
	{
		return m_Node.Count();
	}

	private void SiftUp(int idx)
	{
		int i = idx;
		int parentIdx;
		while (i > 0)
		{
			parentIdx = (i - 1) / 2;
			if (m_Key[i] < m_Key[parentIdx])
			{
				Swap(i, parentIdx);
				i = parentIdx;
			}
			else
				break;
		}
	}

	private void SiftDown(int idx)
	{
		int i = idx;
		int n = m_Node.Count();
		int left;
		int right;
		int smallest;
		while (true)
		{
			left = i * 2 + 1;
			right = i * 2 + 2;
			smallest = i;
			if (left < n && m_Key[left] < m_Key[smallest])
				smallest = left;
			if (right < n && m_Key[right] < m_Key[smallest])
				smallest = right;
			if (smallest == i)
				break;
			Swap(i, smallest);
			i = smallest;
		}
	}

	private void Swap(int a, int b)
	{
		int tmpNode = m_Node[a];
		float tmpKey = m_Key[a];
		m_Node[a] = m_Node[b];
		m_Key[a] = m_Key[b];
		m_Node[b] = tmpNode;
		m_Key[b] = tmpKey;
	}
}

class dmRoadRouter
{
	static ref dmRoadRouter s_Instance;

	//! Sentinel cost for Dijkstra/A* (larger than any real route length).
	static const float DM_ROUTER_INF = 1000000000.0;
	//! Seam-dedup threshold: two consecutive waypoints closer than this (XZ) are
	//! considered the same node re-emitted (bit-identical positions) and dropped.
	static const float DM_ROUTER_DUP_EPS_SQ = 0.0001;

	static const int PHASE_START = 0;
	static const int PHASE_LEGS = 1;
	static const int PHASE_END = 2;
	static const int PHASE_DONE = 3;

	static dmRoadRouter Get()
	{
		if (!s_Instance)
			s_Instance = new dmRoadRouter();
		return s_Instance;
	}

	private bool m_Active;
	private int m_Phase;
	private vector m_StartPos;
	private vector m_TargetPos;
	private int m_StartFull;
	private int m_TargetFull;
	private int m_StartCoarse;
	private int m_TargetCoarse;
	private ref array<int> m_CoarseRoute;
	private int m_RouteIdx;
	private int m_LastBoundary;
	private vector m_LastEmittedPos;
	private bool m_HasEmitted;

	void dmRoadRouter()
	{
		m_Active = false;
		m_Phase = PHASE_DONE;
		m_StartPos = vector.Zero;
		m_TargetPos = vector.Zero;
		m_StartFull = -1;
		m_TargetFull = -1;
		m_StartCoarse = -1;
		m_TargetCoarse = -1;
		m_CoarseRoute = null;
		m_RouteIdx = 0;
		m_LastBoundary = -1;
		m_LastEmittedPos = vector.Zero;
		m_HasEmitted = false;
	}

	//! Nearest coarse (simplified) graph node to `pos` by XZ distance.
	int SnapToCoarse(vector pos)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.Snap");
		#endif

		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<vector> cpos = mgr.GetCoarsePos();
		if (!cpos || cpos.Count() == 0)
			return -1;

		int best = -1;
		float bestSq = DM_ROUTER_INF;
		float dx;
		float dz;
		float dSq;
		int i;
		for (i = 0; i < cpos.Count(); i++)
		{
			dx = cpos[i][0] - pos[0];
			dz = cpos[i][2] - pos[2];
			dSq = dx * dx + dz * dz;
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = i;
			}
		}
		return best;
	}

	//! Nearest full-graph node to `pos` by XZ distance (linear scan over 44k).
	int SnapToFull(vector pos)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.Snap");
		#endif

		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<vector> fpos = mgr.GetFullPos();
		if (!fpos || fpos.Count() == 0)
			return -1;

		int best = -1;
		float bestSq = DM_ROUTER_INF;
		float dx;
		float dz;
		float dSq;
		int i;
		for (i = 0; i < fpos.Count(); i++)
		{
			dx = fpos[i][0] - pos[0];
			dz = fpos[i][2] - pos[2];
			dSq = dx * dx + dz * dz;
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = i;
			}
		}
		return best;
	}

	//! Coarse route from `fromCoarse` to `toCoarse` (inclusive node ids) via a
	//! naive O(V^2) Dijkstra (no heap — the coarse graph has ~486 nodes).
	//! Returns an empty array when no path exists.
	array<int> FindCoarseRoute(int fromCoarse, int toCoarse)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.CoarseDijkstra");
		#endif

		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<int> result = new array<int>();
		array<int> rev = new array<int>();
		array<int> coff = mgr.GetCoarseAdjOffset();
		array<int> ctgt = mgr.GetCoarseAdjTarget();
		array<float> cw = mgr.GetCoarseAdjWeight();
		if (!coff || !ctgt || !cw)
			return result;
		int n = coff.Count() - 1;

		if (fromCoarse < 0 || fromCoarse >= n || toCoarse < 0 || toCoarse >= n)
			return result;
		if (fromCoarse == toCoarse)
		{
			result.Insert(fromCoarse);
			return result;
		}

		array<float> dist = new array<float>();
		array<int> parent = new array<int>();
		array<bool> settled = new array<bool>();
		int i;
		for (i = 0; i < n; i++)
		{
			dist.Insert(DM_ROUTER_INF);
			parent.Insert(-1);
			settled.Insert(false);
		}
		dist[fromCoarse] = 0.0;

		int u;
		int v;
		int j;
		int cur;
		float bestD;
		float nd;
		while (true)
		{
			u = -1;
			bestD = DM_ROUTER_INF;
			for (i = 0; i < n; i++)
			{
				if (!settled[i] && dist[i] < bestD)
				{
					bestD = dist[i];
					u = i;
				}
			}
			if (u < 0)
				break;
			if (u == toCoarse)
				break;
			settled[u] = true;
			for (j = coff[u]; j < coff[u + 1]; j++)
			{
				v = ctgt[j];
				if (settled[v])
					continue;
				nd = dist[u] + cw[j];
				if (nd < dist[v])
				{
					dist[v] = nd;
					parent[v] = u;
				}
			}
		}

		if (dist[toCoarse] >= DM_ROUTER_INF)
			return result;

		cur = toCoarse;
		while (cur >= 0)
		{
			rev.Insert(cur);
			if (cur == fromCoarse)
				break;
			cur = parent[cur];
		}
		for (i = rev.Count() - 1; i >= 0; i--)
			result.Insert(rev[i]);
		return result;
	}

	//! Exact shortest path over the full graph (A* with a binary min-heap and a
	//! Euclidean-distance heuristic). Returns node ids including both ends, empty
	//! when no path exists. Used for short settlement transits (b_in -> b_out).
	array<int> FindFullPath(int fromFull, int toFull)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.FullAStar");
		#endif

		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<int> result = new array<int>();
		array<vector> fpos = mgr.GetFullPos();
		array<int> foff = mgr.GetFullAdjOffset();
		array<int> ftgt = mgr.GetFullAdjTarget();
		array<float> fw = mgr.GetFullAdjWeight();
		if (!fpos || !foff || !ftgt || !fw)
			return result;
		int n = foff.Count() - 1;

		if (fromFull < 0 || fromFull >= n || toFull < 0 || toFull >= n)
			return result;
		if (fromFull == toFull)
		{
			result.Insert(fromFull);
			return result;
		}

		vector goalPos = fpos[toFull];

		ref map<int, float> g = new map<int, float>();
		ref map<int, int> parent = new map<int, int>();
		ref map<int, bool> settled = new map<int, bool>();
		dmRouterMinHeap heap = new dmRouterMinHeap();

		g.Set(fromFull, 0.0);
		heap.Push(fromFull, vector.Distance(fpos[fromFull], goalPos));

		int u;
		int v;
		int j;
		int cur;
		int p;
		int i;
		float poppedF;
		float gu;
		float gv;
		float nd;
		float hVal;
		bool done;
		bool vDone;
		vector vp;

		while (heap.Count() > 0)
		{
			if (!heap.Pop(u, poppedF))
				break;
			done = false;
			if (settled.Find(u, done) && done)
				continue;
			settled.Set(u, true);
			if (u == toFull)
				break;

			gu = 0.0;
			g.Find(u, gu);

			for (j = foff[u]; j < foff[u + 1]; j++)
			{
				v = ftgt[j];
				vDone = false;
				if (settled.Find(v, vDone) && vDone)
					continue;
				gv = DM_ROUTER_INF;
				g.Find(v, gv);
				nd = gu + fw[j];
				if (nd < gv)
				{
					g.Set(v, nd);
					parent.Set(v, u);
					vp = fpos[v];
					hVal = vector.Distance(vp, goalPos);
					heap.Push(v, nd + hVal);
				}
			}
		}

		if (!settled.Contains(toFull))
			return result;

		array<int> rev = new array<int>();
		cur = toFull;
		while (true)
		{
			rev.Insert(cur);
			if (cur == fromFull)
				break;
			p = -1;
			if (!parent.Find(cur, p) || p < 0)
			{
				rev.Clear();
				return result;
			}
			cur = p;
		}
		for (i = rev.Count() - 1; i >= 0; i--)
			result.Insert(rev[i]);

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROUTER] AStar " + fromFull + "->" + toFull + " n=" + result.Count());
		#endif
		return result;
	}

	//! Snap start/target onto both graphs and precompute the coarse route.
	//! Returns false when either snap fails or no coarse route exists.
	bool Setup(vector startPos, vector targetPos)
	{
		m_Active = false;
		m_StartPos = startPos;
		m_TargetPos = targetPos;

		m_StartFull = SnapToFull(startPos);
		m_TargetFull = SnapToFull(targetPos);
		m_StartCoarse = SnapToCoarse(startPos);
		m_TargetCoarse = SnapToCoarse(targetPos);

		if (m_StartFull < 0 || m_TargetFull < 0 || m_StartCoarse < 0 || m_TargetCoarse < 0)
			return false;

		m_CoarseRoute = FindCoarseRoute(m_StartCoarse, m_TargetCoarse);
		if (!m_CoarseRoute || m_CoarseRoute.Count() == 0)
			return false;

		m_RouteIdx = 0;
		m_LastBoundary = m_StartFull;
		m_LastEmittedPos = vector.Zero;
		m_HasEmitted = false;
		m_Phase = PHASE_START;
		m_Active = true;

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROUTER] Setup startCoarse=" + m_StartCoarse + " targetCoarse=" + m_TargetCoarse + " legs=" + m_CoarseRoute.Count());
		dmBotLog.Debug("[ROUTER] Setup startFull=" + m_StartFull + " targetFull=" + m_TargetFull);
		#endif
		return true;
	}

	//! Emit the next chunk of the exact route (cleared first). Each call emits the
	//! start segment once, then up to `lookahead` coarse transitions (legs, with
	//! settlement transits between them), and finally the end segment when the
	//! route is exhausted. Returns true while more chunks remain, false once the
	//! target is reached.
	bool NextChunk(inout array<vector> waypoints, int lookahead)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.NextChunk");
		#endif

		if (!m_Active)
			return false;

		if (lookahead < 1)
			lookahead = 1;

		waypoints.Clear();

		int emittedLegs = 0;

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROUTER] NextChunk phase=" + m_Phase + " routeIdx=" + m_RouteIdx + " lookahead=" + lookahead);
		#endif

		if (m_Phase == PHASE_START)
		{
			EmitStart(waypoints);
			m_Phase = PHASE_LEGS;
		}

		while (m_Phase == PHASE_LEGS && emittedLegs < lookahead && m_RouteIdx < m_CoarseRoute.Count() - 1)
		{
			EmitLeg(waypoints);
			emittedLegs = emittedLegs + 1;
		}

		if (m_Phase == PHASE_LEGS && m_RouteIdx >= m_CoarseRoute.Count() - 1)
			m_Phase = PHASE_END;

		if (m_Phase == PHASE_END)
		{
			EmitEnd(waypoints);
			m_Phase = PHASE_DONE;
			m_Active = false;
			return false;
		}

		return true;
	}

	//! Start segment: off-road start -> nearest full-graph node (on-road entry).
	private void EmitStart(inout array<vector> waypoints)
	{
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<vector> fpos = mgr.GetFullPos();
		EmitPos(m_StartPos, waypoints);
		EmitPos(fpos[m_StartFull], waypoints);
		m_LastBoundary = m_StartFull;
	}

	//! End segment: off-ramp from the last leg boundary to the target full node,
	//! then the off-road target.
	private void EmitEnd(inout array<vector> waypoints)
	{
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<vector> fpos = mgr.GetFullPos();
		if (m_LastBoundary != m_TargetFull)
			EmitFullPathPositions(m_LastBoundary, m_TargetFull, waypoints);
		EmitPos(fpos[m_TargetFull], waypoints);
		EmitPos(m_TargetPos, waypoints);
	}

	//! Emit one coarse transition (Ci -> Ci+1): an on-ramp/settlement transit
	//! (when the current frontier boundary differs from this leg's entry), then
	//! the leg polyline. Advances m_RouteIdx and m_LastBoundary.
	private void EmitLeg(inout array<vector> waypoints)
	{
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		int ci = m_CoarseRoute[m_RouteIdx];
		int ci1 = m_CoarseRoute[m_RouteIdx + 1];
		int edgeIdx = FindCoarseEdge(ci, ci1);
		if (edgeIdx < 0)
		{
			m_RouteIdx = m_RouteIdx + 1;
			return;
		}

		dmSimplifiedEdge edge = mgr.GetCoarseEdges()[edgeIdx];
		array<int> legPath = mgr.GetCoarsePaths()[edgeIdx];
		if (!edge || !legPath || legPath.Count() == 0)
		{
			m_RouteIdx = m_RouteIdx + 1;
			return;
		}

		bool forward = (edge.From == ci);

		int lastIdx = legPath.Count() - 1;
		int bOut;
		int bInNext;
		if (forward)
		{
			bOut = legPath[0];
			bInNext = legPath[lastIdx];
		}
		else
		{
			bOut = legPath[lastIdx];
			bInNext = legPath[0];
		}

		bool needTransit = false;
		if (m_RouteIdx == 0)
			needTransit = (m_LastBoundary != bOut);
		else
			needTransit = (IsSettlement(ci) && m_LastBoundary != bOut);

		if (needTransit)
			EmitFullPathPositions(m_LastBoundary, bOut, waypoints);

		EmitPathPositions(legPath, forward, waypoints);

		m_LastBoundary = bInNext;
		m_RouteIdx = m_RouteIdx + 1;
	}

	//! True when coarse node `id` is a settlement cluster (multi-member vertex),
	//! where consecutive legs meet at different boundary nodes and need a transit.
	private bool IsSettlement(int id)
	{
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<ref dmSimplifiedNode> nodes = mgr.GetCoarseNodes();
		if (!nodes || id < 0 || id >= nodes.Count())
			return false;
		return nodes[id].Type == "settlement";
	}

	//! Edge index of the coarse adjacency entry a->b (parallel m_CoarseAdjEdge),
	//! or -1 when absent.
	private int FindCoarseEdge(int a, int b)
	{
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<int> coff = mgr.GetCoarseAdjOffset();
		array<int> ctgt = mgr.GetCoarseAdjTarget();
		array<int> cedge = mgr.GetCoarseAdjEdge();
		int j;
		for (j = coff[a]; j < coff[a + 1]; j++)
		{
			if (ctgt[j] == b)
				return cedge[j];
		}
		return -1;
	}

	//! Expand an A* result (full node ids) into positions and emit them.
	private void EmitFullPathPositions(int fromNode, int toNode, inout array<vector> waypoints)
	{
		array<int> fullPath = FindFullPath(fromNode, toNode);
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<vector> fpos = mgr.GetFullPos();
		int i;
		for (i = 0; i < fullPath.Count(); i++)
			EmitPos(fpos[fullPath[i]], waypoints);
	}

	//! Expand a leg's Path (full node ids along the contracted chain) into
	//! positions, in traversal order (reversed when the edge is stored backward).
	private void EmitPathPositions(array<int> legPath, bool forward, inout array<vector> waypoints)
	{
		dmRoadGraphManager mgr = dmRoadGraphManager.Get();
		array<vector> fpos = mgr.GetFullPos();
		int n = legPath.Count();
		int i;
		int idx;
		for (i = 0; i < n; i++)
		{
			idx = i;
			if (!forward)
				idx = n - 1 - i;
			EmitPos(fpos[legPath[idx]], waypoints);
		}
	}

	//! Append one waypoint, dropping it when it duplicates the previous one (seam
	//! dedup, XZ distance below DM_ROUTER_DUP_EPS_SQ). Crosses chunk boundaries
	//! via m_LastEmittedPos/m_HasEmitted.
	private void EmitPos(vector p, inout array<vector> waypoints)
	{
		vector refPos = p;
		bool haveRef = false;
		if (waypoints.Count() > 0)
		{
			refPos = waypoints[waypoints.Count() - 1];
			haveRef = true;
		}
		else if (m_HasEmitted)
		{
			refPos = m_LastEmittedPos;
			haveRef = true;
		}

		if (haveRef)
		{
			float dx = refPos[0] - p[0];
			float dz = refPos[2] - p[2];
			float dSq = dx * dx + dz * dz;
			if (dSq < DM_ROUTER_DUP_EPS_SQ)
				return;
		}

		waypoints.Insert(p);
		m_LastEmittedPos = p;
		m_HasEmitted = true;
	}
}
