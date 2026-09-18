//! dmRoadDiscoveryManager — whole-map road discovery (tiles, resumable, merge).
//!
//! Drives dmRoadObjectGraphBuilder over the whole map as a grid of 1x1 km
//! tiles, one tile per server tick (a synchronous Build). After every tile the
//! progress checkpoint (discovery_progress.json) is rewritten so a restart
//! resumes from where it left off. Once every tile is scanned, the tiles are
//! merged into one road_graph.json (vertices renumbered, tile-boundary vertices
//! sewn by proximity) plus road_gaps.json, and a done marker is written so the
//! whole pass never reruns. Auto-started from MissionServer.OnUpdate under
//! DM_BOT_DISCOVERY (the whole Tick body is gated — the map module calls it
//! unconditionally).

//! Checkpoint of the scan: the set of already-scanned tile keys ("tx:tz").
class dmRoadDiscoveryProgress
{
	int Version;
	string WorldName;
	ref array<string> DoneTiles;
}

class dmRoadDiscoveryManager
{
	static ref dmRoadDiscoveryManager s_Instance;

	static dmRoadDiscoveryManager Get()
	{
		if (!s_Instance)
			s_Instance = new dmRoadDiscoveryManager();
		return s_Instance;
	}

	private bool m_Init;
	private bool m_Scanning;
	private bool m_Merged;
	private ref dmRoadDiscoveryProgress m_Progress;
	private ref array<string> m_Tiles;
	private int m_TileIndex;

	void dmRoadDiscoveryManager()
	{
		m_Init = false;
		m_Scanning = false;
		m_Merged = false;
		m_Progress = null;
		m_Tiles = null;
		m_TileIndex = 0;
	}

	void Tick(float timeslice)
	{
		#ifdef DM_BOT_DISCOVERY
		if (m_Merged)
			return;

		if (!m_Init)
		{
			m_Init = true;
			Init();
		}
		if (m_Merged)
			return;

		ScanStep();
		#endif
	}

	//! First-tick setup: stop forever when the done marker exists, else load the
	//! checkpoint and compute the full tile list. Sets m_Merged (done) or
	//! m_Scanning (work to do).
	private void Init()
	{
		if (FileExist(DM_ROADS_DONE_FILE))
		{
			m_Merged = true;
			return;
		}

		m_Progress = new dmRoadDiscoveryProgress();
		m_Progress.Version = 1;
		m_Progress.WorldName = GetGame().GetWorldName();
		m_Progress.DoneTiles = new array<string>();

		LoadProgress();
		ComputeTiles();

		m_Scanning = true;
		m_TileIndex = 0;

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[DISCOVERY] tiles=" + m_Tiles.Count() + " done=" + m_Progress.DoneTiles.Count());
		#endif
	}

	//! Merge the done-tile keys from a previous run into the fresh checkpoint.
	//! A missing/corrupt file leaves the checkpoint empty (full rescan).
	private void LoadProgress()
	{
		if (!FileExist(DM_ROADS_PROGRESS_FILE))
			return;

		dmRoadDiscoveryProgress loaded;
		string error;
		if (!JsonFileLoader<dmRoadDiscoveryProgress>.LoadFile(DM_ROADS_PROGRESS_FILE, loaded, error))
		{
			dmBotLog.Error("[DISCOVERY] progress load failed: " + error);
			return;
		}
		if (!loaded || !loaded.DoneTiles)
			return;

		int i;
		for (i = 0; i < loaded.DoneTiles.Count(); i++)
			m_Progress.DoneTiles.Insert(loaded.DoneTiles[i]);
	}

	//! Full tile grid over the map: ceil(worldSize / DM_ROAD_TILE_SIZE) tiles
	//! per axis, keyed "tx:tz".
	private void ComputeTiles()
	{
		float worldSize = GetGame().ConfigGetFloat("CfgWorlds " + GetGame().GetWorldName() + " worldSize");
		if (worldSize <= 0.0)
			worldSize = 15360.0;

		int tilesPerAxis = Math.Ceil(worldSize / DM_ROAD_TILE_SIZE);

		m_Tiles = new array<string>();
		int tx;
		int tz;
		for (tx = 0; tx < tilesPerAxis; tx++)
		{
			for (tz = 0; tz < tilesPerAxis; tz++)
				m_Tiles.Insert(tx.ToString() + ":" + tz.ToString());
		}
	}

	//! Scan at most one unfinished tile this tick, checkpoint, then return. When
	//! every tile is done, run the one-time merge.
	private void ScanStep()
	{
		string key;
		while (m_TileIndex < m_Tiles.Count())
		{
			key = m_Tiles[m_TileIndex];
			if (IsDone(key))
			{
				m_TileIndex = m_TileIndex + 1;
				continue;
			}
			ScanTile(key);
			m_Progress.DoneTiles.Insert(key);
			SaveProgress();
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[DISCOVERY] tile " + key + " done=" + m_Progress.DoneTiles.Count() + "/" + m_Tiles.Count());
			#endif
			m_TileIndex = m_TileIndex + 1;
			return;
		}
		Merge();
	}

	//! True when the tile key is already in the checkpoint.
	private bool IsDone(string key)
	{
		int i;
		for (i = 0; i < m_Progress.DoneTiles.Count(); i++)
		{
			if (m_Progress.DoneTiles[i] == key)
				return true;
		}
		return false;
	}

	//! Build the partial graph for one tile and save it to tiles/<key>.json.
	private void ScanTile(string key)
	{
		int tx;
		int tz;
		ParseKey(key, tx, tz);

		vector min = Vector(tx * DM_ROAD_TILE_SIZE, 0.0, tz * DM_ROAD_TILE_SIZE);
		vector max = Vector(min[0] + DM_ROAD_TILE_SIZE, 0.0, min[2] + DM_ROAD_TILE_SIZE);

		dmRoadGraph partial = dmRoadObjectGraphBuilder.Build(min, max);
		dmRoadGraphIO.Save(partial, DM_ROADS_TILES_DIR + "/" + key + ".json");
	}

	//! Split a tile key "tx:tz" into its two integer coordinates.
	private void ParseKey(string key, out int tx, out int tz)
	{
		tx = 0;
		tz = 0;
		TStringArray parts = new TStringArray();
		key.Split(":", parts);
		if (parts.Count() == 2)
		{
			tx = parts[0].ToInt();
			tz = parts[1].ToInt();
		}
	}

	//! Rewrite the checkpoint file.
	private void SaveProgress()
	{
		EnsureDirectory(DM_ROADS_PROGRESS_FILE);
		string error;
		if (!JsonFileLoader<dmRoadDiscoveryProgress>.SaveFile(DM_ROADS_PROGRESS_FILE, m_Progress, error))
			dmBotLog.Error("[DISCOVERY] progress save failed: " + error);
	}

	//! One-time final step: merge every tile graph into one, sew coincident
	//! boundary vertices, reclassify, detect gaps and write the outputs.
	private void Merge()
	{
		m_Scanning = false;
		m_Merged = true;

		dmRoadGraph merged = new dmRoadGraph();
		merged.Nodes = new array<ref dmRoadGraphNode>();
		merged.Edges = new array<ref dmRoadGraphEdge>();

		int nodeOffset = 0;
		int edgeOffset = 0;
		string error;
		dmRoadGraph tile;
		int k;
		for (k = 0; k < m_Progress.DoneTiles.Count(); k++)
		{
			string key = m_Progress.DoneTiles[k];
			if (!JsonFileLoader<dmRoadGraph>.LoadFile(DM_ROADS_TILES_DIR + "/" + key + ".json", tile, error))
			{
				dmBotLog.Error("[DISCOVERY] tile load failed: " + key + ": " + error);
				continue;
			}
			AppendTile(merged, tile, nodeOffset, edgeOffset);
			nodeOffset = nodeOffset + tile.Nodes.Count();
			edgeOffset = edgeOffset + tile.Edges.Count();
		}

		SewNodes(merged);
		ClassifyNodes(merged);

		dmRoadGapList gaps = dmRoadGapDetector.Detect(merged);

		dmRoadGraphIO.Save(merged, DM_ROADS_GRAPH_FILE);
		dmRoadGapIO.Save(gaps, DM_ROADS_GAP_FILE);
		WriteMarker();

		dmBotLog.Error("[DISCOVERY] COMPLETE nodes=" + merged.Nodes.Count() + " edges=" + merged.Edges.Count() + " gaps=" + gaps.Gaps.Count());
	}

	//! Append one tile's nodes/edges into the merged graph, renumbering their ids
	//! by the running node/edge offsets.
	private void AppendTile(dmRoadGraph merged, dmRoadGraph tile, int nodeOffset, int edgeOffset)
	{
		int i;
		for (i = 0; i < tile.Nodes.Count(); i++)
		{
			tile.Nodes[i].Id = tile.Nodes[i].Id + nodeOffset;
			merged.Nodes.Insert(tile.Nodes[i]);
		}
		for (i = 0; i < tile.Edges.Count(); i++)
		{
			tile.Edges[i].Id = tile.Edges[i].Id + edgeOffset;
			tile.Edges[i].From = tile.Edges[i].From + nodeOffset;
			tile.Edges[i].To = tile.Edges[i].To + nodeOffset;
			merged.Edges.Insert(tile.Edges[i]);
		}
	}

	//! Sew vertices whose XZ positions coincide (tile-boundary seams): union-find
	//! over a spatial hash of cells, then rebuild the graph with merged vertices.
	//! O(n) — the previous pairwise pass (O(n^2)) froze the server on ~44k nodes.
	private static void SewNodes(dmRoadGraph graph)
	{
		array<int> parent = new array<int>();
		int i;
		int j;
		int dx;
		int dz;
		int ci;
		int cj;
		int oi;
		int r;
		int newId;
		int f;
		int t;
		int eid;
		string key;
		array<int> bucket;
		ref map<string, ref array<int>> grid = new map<string, ref array<int>>();
		ref map<int, int> rootToNew = new map<int, int>();
		array<ref dmRoadGraphNode> kept = new array<ref dmRoadGraphNode>();
		array<ref dmRoadGraphEdge> keptEdges = new array<ref dmRoadGraphEdge>();

		for (i = 0; i < graph.Nodes.Count(); i++)
			parent.Insert(i);

		for (i = 0; i < graph.Nodes.Count(); i++)
		{
			key = CellKey(graph.Nodes[i].Pos, DM_ROAD_ENDPOINT_SNAP);
			if (!grid.Find(key, bucket))
			{
				bucket = new array<int>();
				grid.Insert(key, bucket);
			}
			bucket.Insert(i);
		}

		for (i = 0; i < graph.Nodes.Count(); i++)
		{
			ci = Math.Floor(graph.Nodes[i].Pos[0] / DM_ROAD_ENDPOINT_SNAP);
			cj = Math.Floor(graph.Nodes[i].Pos[2] / DM_ROAD_ENDPOINT_SNAP);
			for (dx = -1; dx <= 1; dx++)
			{
				for (dz = -1; dz <= 1; dz++)
				{
					key = (ci + dx).ToString() + ":" + (cj + dz).ToString();
					if (!grid.Find(key, bucket))
						continue;
					for (j = 0; j < bucket.Count(); j++)
					{
						oi = bucket[j];
						if (oi <= i)
							continue;
						if (Find(parent, i) == Find(parent, oi))
							continue;
						if (CloseXZ(graph.Nodes[i].Pos, graph.Nodes[oi].Pos))
							Union(parent, i, oi);
					}
				}
			}
		}

		for (i = 0; i < graph.Nodes.Count(); i++)
		{
			r = Find(parent, i);
			if (!rootToNew.Find(r, newId))
			{
				newId = kept.Count();
				rootToNew.Insert(r, newId);
				graph.Nodes[i].Id = newId;
				kept.Insert(graph.Nodes[i]);
			}
		}
		graph.Nodes = kept;

		eid = 0;
		for (i = 0; i < graph.Edges.Count(); i++)
		{
			if (!rootToNew.Find(Find(parent, graph.Edges[i].From), f))
				continue;
			if (!rootToNew.Find(Find(parent, graph.Edges[i].To), t))
				continue;
			if (f == t)
				continue;
			if (HasEdge(keptEdges, f, t))
				continue;
			graph.Edges[i].Id = eid;
			graph.Edges[i].From = f;
			graph.Edges[i].To = t;
			keptEdges.Insert(graph.Edges[i]);
			eid = eid + 1;
		}
		graph.Edges = keptEdges;
	}

	//! True when two points are within DM_ROAD_ENDPOINT_SNAP in the XZ plane.
	private static bool CloseXZ(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz <= DM_ROAD_ENDPOINT_SNAP * DM_ROAD_ENDPOINT_SNAP;
	}

	//! Union-find representative with path halving (matches merge_road_tiles.py).
	private static int Find(array<int> parent, int x)
	{
		while (parent[x] != x)
		{
			parent[x] = parent[parent[x]];
			x = parent[x];
		}
		return x;
	}

	//! Union the sets of a and b, merging into the earlier (a's) representative.
	private static void Union(array<int> parent, int a, int b)
	{
		int ra = Find(parent, a);
		int rb = Find(parent, b);
		if (ra != rb)
			parent[rb] = ra;
	}

	//! Spatial-hash cell key "ci:cj" for a point, using the given cell size.
	private static string CellKey(vector pos, float cell)
	{
		int ci = Math.Floor(pos[0] / cell);
		int cj = Math.Floor(pos[2] / cell);
		return ci.ToString() + ":" + cj.ToString();
	}

	//! True when an edge already connects a and b in either direction.
	private static bool HasEdge(array<ref dmRoadGraphEdge> edges, int a, int b)
	{
		int i;
		for (i = 0; i < edges.Count(); i++)
		{
			if ((edges[i].From == a && edges[i].To == b) || (edges[i].From == b && edges[i].To == a))
				return true;
		}
		return false;
	}

	//! Recompute every vertex Kind from its degree: >=3 junction, 2 bend,
	//! 1 deadend, 0 isolated. Mirrors dmRoadObjectGraphBuilder.ClassifyNodes.
	private static void ClassifyNodes(dmRoadGraph graph)
	{
		int i;
		int j;
		int deg;
		for (i = 0; i < graph.Nodes.Count(); i++)
		{
			deg = 0;
			for (j = 0; j < graph.Edges.Count(); j++)
			{
				if (graph.Edges[j].From == graph.Nodes[i].Id)
					deg = deg + 1;
				if (graph.Edges[j].To == graph.Nodes[i].Id)
					deg = deg + 1;
			}
			if (deg >= 3)
				graph.Nodes[i].Kind = "junction";
			else if (deg == 2)
				graph.Nodes[i].Kind = "bend";
			else if (deg == 1)
				graph.Nodes[i].Kind = "deadend";
			else
				graph.Nodes[i].Kind = "isolated";
		}
	}

	//! Write the empty completion marker file.
	private void WriteMarker()
	{
		EnsureDirectory(DM_ROADS_DONE_FILE);
		FileHandle file = OpenFile(DM_ROADS_DONE_FILE, FileMode.WRITE);
		if (file)
			CloseFile(file);
	}

	//! Create the parent directory chain of a file path (handles the "$profile:"
	//! prefix and relative paths alike).
	private static void EnsureDirectory(string path)
	{
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash < 0)
			return;

		TStringArray comps = new TStringArray();
		path.Substring(0, lastSlash).Split("/", comps);

		int startFrom = 0;
		string dir = "";
		if (comps.Count() > 0 && comps[0] == "$profile:")
		{
			dir = "$profile:";
			startFrom = 1;
		}

		int i;
		for (i = startFrom; i < comps.Count(); i++)
		{
			if (dir != "")
				dir += "/";
			dir += comps[i];
			if (!FileExist(dir))
				MakeDirectory(dir);
		}
	}
}
