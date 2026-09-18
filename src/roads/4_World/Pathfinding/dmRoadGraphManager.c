//! dmRoadGraphManager — runtime road-graph loader + CSR builder (Dijkstra-ready).
//!
//! Loads the two offline JSON artifacts (the full road graph and its simplified
//! coarse skeleton) on the first server tick and converts each into a
//! compressed-sparse-row (CSR) adjacency structure optimized for Dijkstra. CSR is
//! rebuilt from JSON on every server start — nothing is persisted. Idempotent:
//! the first Tick() runs LoadAndBuild(), every later call is a no-op (m_Loaded).
//! Auto-started from MissionServer.OnUpdate (not gated by DM_BOT_DISCOVERY — the
//! graph is always loaded).

class dmRoadGraphManager
{
	static ref dmRoadGraphManager s_Instance;

	static dmRoadGraphManager Get()
	{
		if (!s_Instance)
			s_Instance = new dmRoadGraphManager();
		return s_Instance;
	}

	private bool m_Loaded;

	//! CSR of the full graph: node positions + adjacency (weight = edge length).
	private ref array<vector> m_FullPos;
	private ref array<int> m_FullAdjOffset;
	private ref array<int> m_FullAdjTarget;
	private ref array<float> m_FullAdjWeight;

	//! CSR of the coarse graph: node positions + adjacency, plus the coarse
	//! vertices (Type/Weight/Radius/Members) and per-edge provenance Path.
	private ref array<vector> m_CoarsePos;
	private ref array<int> m_CoarseAdjOffset;
	private ref array<int> m_CoarseAdjTarget;
	private ref array<float> m_CoarseAdjWeight;
	private ref array<ref dmSimplifiedNode> m_CoarseNodes;
	private ref array<ref array<int>> m_CoarsePaths;

	void dmRoadGraphManager()
	{
		m_Loaded = false;
		m_FullPos = null;
		m_FullAdjOffset = null;
		m_FullAdjTarget = null;
		m_FullAdjWeight = null;
		m_CoarsePos = null;
		m_CoarseAdjOffset = null;
		m_CoarseAdjTarget = null;
		m_CoarseAdjWeight = null;
		m_CoarseNodes = null;
		m_CoarsePaths = null;
	}

	//! Idempotent entry: first call loads+builds once, every later call is a no-op.
	void Tick(float timeslice)
	{
		if (m_Loaded)
			return;
		m_Loaded = true;
		LoadAndBuild();
	}

	//! One-time: load both JSON artifacts, build their CSR forms, log a summary.
	private void LoadAndBuild()
	{
		dmRoadGraph full;
		dmSimplifiedGraph coarse;
		if (!LoadGraphs(full, coarse))
			return;

		BuildFullCSR(full);
		BuildCoarseCSR(coarse);

		dmBotLog.Error("[ROADS] full=" + full.Nodes.Count() + "n/" + full.Edges.Count() + "e");
		dmBotLog.Error("[ROADS] coarse=" + coarse.Nodes.Count() + "n/" + coarse.Edges.Count() + "e");

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROADS] CSR full adj=" + m_FullAdjTarget.Count() + " coarse adj=" + m_CoarseAdjTarget.Count());
		#endif
	}

	//! Load both JSON artifacts. A missing/corrupt file logs an error and aborts
	//! (the runtime simply runs without road graphs — the server stays up).
	private bool LoadGraphs(out dmRoadGraph full, out dmSimplifiedGraph coarse)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.Load");
		#endif

		string error;
		if (!JsonFileLoader<dmRoadGraph>.LoadFile(DM_ROADS_GRAPH_FILE, full, error))
		{
			dmBotLog.Error("[ROADS] full graph load failed: " + error);
			return false;
		}
		if (!full || !full.Nodes || !full.Edges)
		{
			dmBotLog.Error("[ROADS] full graph empty/malformed: " + DM_ROADS_GRAPH_FILE);
			return false;
		}
		if (!JsonFileLoader<dmSimplifiedGraph>.LoadFile(DM_ROADS_SIMPLIFIED_FILE, coarse, error))
		{
			dmBotLog.Error("[ROADS] simplified graph load failed: " + error);
			return false;
		}
		if (!coarse || !coarse.Nodes || !coarse.Edges)
		{
			dmBotLog.Error("[ROADS] simplified graph empty/malformed: " + DM_ROADS_SIMPLIFIED_FILE);
			return false;
		}
		return true;
	}

	//! Build the full graph's CSR in two passes (degrees, then fill). Each
	//! undirected edge contributes two adjacency entries (weight = edge length).
	//! Node ids are contiguous 0..N-1, so positions/offsets are indexed directly
	//! by id. O(nodes + edges).
	//! NOTE: this is the seam where a precomputed binary could be loaded instead
	//! of rebuilding CSR from JSON — nothing is saved to disk today.
	private void BuildFullCSR(dmRoadGraph graph)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.BuildFullCSR");
		#endif

		int nodeCount = graph.Nodes.Count();
		int edgeCount = graph.Edges.Count();
		int i;
		int from;
		int to;
		int c;
		int running;
		int next;
		float length;

		m_FullPos = new array<vector>();
		m_FullAdjOffset = new array<int>();
		m_FullAdjTarget = new array<int>();
		m_FullAdjWeight = new array<float>();

		for (i = 0; i < nodeCount; i++)
		{
			m_FullPos.Insert(graph.Nodes[i].Pos);
			m_FullAdjOffset.Insert(0);
		}
		m_FullAdjOffset.Insert(0);

		for (i = 0; i < edgeCount; i++)
		{
			m_FullAdjOffset[graph.Edges[i].From] = m_FullAdjOffset[graph.Edges[i].From] + 1;
			m_FullAdjOffset[graph.Edges[i].To] = m_FullAdjOffset[graph.Edges[i].To] + 1;
		}

		running = 0;
		for (i = 0; i <= nodeCount; i++)
		{
			next = running + m_FullAdjOffset[i];
			m_FullAdjOffset[i] = running;
			running = next;
		}

		for (i = 0; i < running; i++)
		{
			m_FullAdjTarget.Insert(0);
			m_FullAdjWeight.Insert(0.0);
		}

		array<int> cursor = new array<int>();
		for (i = 0; i <= nodeCount; i++)
			cursor.Insert(m_FullAdjOffset[i]);

		for (i = 0; i < edgeCount; i++)
		{
			from = graph.Edges[i].From;
			to = graph.Edges[i].To;
			length = graph.Edges[i].Length;
			c = cursor[from];
			m_FullAdjTarget[c] = to;
			m_FullAdjWeight[c] = length;
			cursor[from] = c + 1;
			c = cursor[to];
			m_FullAdjTarget[c] = from;
			m_FullAdjWeight[c] = length;
			cursor[to] = c + 1;
		}
	}

	//! Build the coarse graph's CSR, mirroring BuildFullCSR, plus the coarse
	//! vertices and per-edge Path provenance (indexed by edge — needed by the fine
	//! routing layer). O(nodes + edges).
	//! NOTE: same seam as above — a precomputed binary could replace JSON->CSR.
	private void BuildCoarseCSR(dmSimplifiedGraph graph)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.BuildCoarseCSR");
		#endif

		int nodeCount = graph.Nodes.Count();
		int edgeCount = graph.Edges.Count();
		int i;
		int from;
		int to;
		int c;
		int running;
		int next;
		float length;

		m_CoarseNodes = graph.Nodes;

		m_CoarsePos = new array<vector>();
		m_CoarseAdjOffset = new array<int>();
		m_CoarseAdjTarget = new array<int>();
		m_CoarseAdjWeight = new array<float>();
		m_CoarsePaths = new array<ref array<int>>();

		for (i = 0; i < nodeCount; i++)
		{
			m_CoarsePos.Insert(graph.Nodes[i].Pos);
			m_CoarseAdjOffset.Insert(0);
		}
		m_CoarseAdjOffset.Insert(0);

		for (i = 0; i < edgeCount; i++)
		{
			m_CoarseAdjOffset[graph.Edges[i].From] = m_CoarseAdjOffset[graph.Edges[i].From] + 1;
			m_CoarseAdjOffset[graph.Edges[i].To] = m_CoarseAdjOffset[graph.Edges[i].To] + 1;
			m_CoarsePaths.Insert(graph.Edges[i].Path);
		}

		running = 0;
		for (i = 0; i <= nodeCount; i++)
		{
			next = running + m_CoarseAdjOffset[i];
			m_CoarseAdjOffset[i] = running;
			running = next;
		}

		for (i = 0; i < running; i++)
		{
			m_CoarseAdjTarget.Insert(0);
			m_CoarseAdjWeight.Insert(0.0);
		}

		array<int> cursor = new array<int>();
		for (i = 0; i <= nodeCount; i++)
			cursor.Insert(m_CoarseAdjOffset[i]);

		for (i = 0; i < edgeCount; i++)
		{
			from = graph.Edges[i].From;
			to = graph.Edges[i].To;
			length = graph.Edges[i].Length;
			c = cursor[from];
			m_CoarseAdjTarget[c] = to;
			m_CoarseAdjWeight[c] = length;
			cursor[from] = c + 1;
			c = cursor[to];
			m_CoarseAdjTarget[c] = from;
			m_CoarseAdjWeight[c] = length;
			cursor[to] = c + 1;
		}
	}
}
