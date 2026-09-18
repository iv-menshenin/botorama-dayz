//! dmRoadObjectGraph — road network graph from road OBJECTS (memory points).
//!
//! Roads are class=="road" objects (dmRoadSensor.IsRoadObject). Each segment has
//! two pairs of end memory points: LB/PB (one end) and LE/PE (the other); the
//! midpoint of a pair is the endpoint of the road centerline. Adjacent segments
//! share endpoints, so the graph is built by scanning a grid for road objects,
//! extracting each segment's two endpoints, snapping coincident endpoints into
//! shared vertices, and connecting them with edges. This replaces the walker
//! (dmRoadProbe) as the primary graph source. See Expansion eAIRoadNode.Generate
//! for the memory-point convention.

//! Grid-scan road-object graph builder.
class dmRoadObjectGraphBuilder
{
	static dmRoadGraph Build(vector min, vector max)
	{
		dmRoadObjectGraphBuilder builder = new dmRoadObjectGraphBuilder();
		return builder.BuildGraph(min, max);
	}

	private ref dmRoadGraph m_Graph;
	private ref map<string, bool> m_Seen;
	private int m_NextNodeId;
	private int m_NextEdgeId;
	private int m_FullSegments;
	private int m_PartialSegments;

	void dmRoadObjectGraphBuilder()
	{
		m_Graph = null;
		m_Seen = new map<string, bool>();
		m_NextNodeId = 0;
		m_NextEdgeId = 0;
		m_FullSegments = 0;
		m_PartialSegments = 0;
	}

	//! Scan the area for road objects, extract each segment's endpoints, snap
	//! them into vertices and connect them with edges.
	private dmRoadGraph BuildGraph(vector min, vector max)
	{
		m_Graph = new dmRoadGraph();
		m_Graph.Nodes = new array<ref dmRoadGraphNode>();
		m_Graph.Edges = new array<ref dmRoadGraphEdge>();

		array<Object> objects = new array<Object>();
		ScanObjects(min, max, objects);

		int i;
		for (i = 0; i < objects.Count(); i++)
			BuildSegment(objects[i]);

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROADNET] segments full=" + m_FullSegments + " partial=" + m_PartialSegments);
		#endif

		ConnectGraph();
		AccumulateMetadata();

		ClassifyNodes();

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROADNET] objects=" + objects.Count() + " nodes=" + m_Graph.Nodes.Count() + " edges=" + m_Graph.Edges.Count());
		#endif

		return m_Graph;
	}

	//! Classify each vertex by its degree: >=3 junction, ==2 bend, ==1 deadend,
	//! 0 isolated. Degree 2 is a plain segment joint (often the asphalt<->dirt
	//! transition), NOT an intersection.
	private void ClassifyNodes()
	{
		int i;
		int j;
		int deg;
		for (i = 0; i < m_Graph.Nodes.Count(); i++)
		{
			deg = 0;
			for (j = 0; j < m_Graph.Edges.Count(); j++)
			{
				if (m_Graph.Edges[j].From == m_Graph.Nodes[i].Id)
					deg = deg + 1;
				if (m_Graph.Edges[j].To == m_Graph.Nodes[i].Id)
					deg = deg + 1;
			}
			if (deg >= 3)
				m_Graph.Nodes[i].Kind = "junction";
			else if (deg == 2)
				m_Graph.Nodes[i].Kind = "bend";
			else if (deg == 1)
				m_Graph.Nodes[i].Kind = "deadend";
			else
				m_Graph.Nodes[i].Kind = "isolated";
		}
	}

	//! Grid-scan [min,max] for road objects, dedup by position cell, and append
	//! each unique road object to outObjects.
	private void ScanObjects(vector min, vector max, array<Object> outObjects)
	{
		float x;
		float z;
		float terrainY;
		array<Object> objs = new array<Object>();
		array<CargoBase> cargos = new array<CargoBase>();
		int i;
		Object obj;
		vector p;
		string key;

		for (x = min[0]; x <= max[0]; x = x + DM_ROAD_SCAN_STEP)
		{
			for (z = min[2]; z <= max[2]; z = z + DM_ROAD_SCAN_STEP)
			{
				#ifdef DM_BOT_DEBUG_ROADS
				dmBotLog.Debug("[ROADNET] grid (" + x + "," + z + ")");
				#endif
				terrainY = GetGame().SurfaceY(x, z);
				objs.Clear();
				cargos.Clear();
				GetGame().GetObjectsAtPosition(Vector(x, terrainY, z), 12.0, objs, cargos);
				#ifdef DM_BOT_DEBUG_ROADS
				dmBotLog.Debug("[ROADNET] (" + x + "," + z + ") objs=" + objs.Count());
				#endif
				for (i = 0; i < objs.Count(); i++)
				{
					obj = objs[i];
					if (!dmRoadSensor.IsRoadObject(obj))
						continue;
					p = obj.GetPosition();
					key = dmRoadProbe.CellKey(p[0], p[2], 2.0);
					if (m_Seen.Contains(key))
						continue;
					m_Seen.Insert(key, true);
					outObjects.Insert(obj);
				}
			}
		}
	}

	//! Turn one road object into a segment (edge) between its two endpoints.
	//! A missing endpoint pair falls back to the object's bbox along its long
	//! axis (local Z) via ClippingInfo, so single-pair stubs still yield a
	//! segment instead of dropping the road (see Expansion eAIRoadNode.Generate).
	private void BuildSegment(Object obj)
	{
		vector mm[2];
		obj.ClippingInfo(mm);
		bool hasA = obj.MemoryPointExists("LB") && obj.MemoryPointExists("PB");
		bool hasB = obj.MemoryPointExists("LE") && obj.MemoryPointExists("PE");
		vector endA;
		vector endB;
		if (hasA)
		{
			vector lb = obj.ModelToWorld(obj.GetMemoryPointPos("LB"));
			vector pb = obj.ModelToWorld(obj.GetMemoryPointPos("PB"));
			endA = (lb + pb) * 0.5;
		}
		else
		{
			endA = obj.ModelToWorld(Vector(0.0, 0.0, mm[0][2]));
		}
		if (hasB)
		{
			vector le = obj.ModelToWorld(obj.GetMemoryPointPos("LE"));
			vector pe = obj.ModelToWorld(obj.GetMemoryPointPos("PE"));
			endB = (le + pe) * 0.5;
		}
		else
		{
			endB = obj.ModelToWorld(Vector(0.0, 0.0, mm[1][2]));
		}
		if (endA == vector.Zero || endB == vector.Zero)
			return;
		int nodeA = FindOrCreateNode(endA, "junction");
		int nodeB = FindOrCreateNode(endB, "junction");
		AddEdge(nodeA, nodeB, endA, endB);
		if (hasA && hasB)
			m_FullSegments = m_FullSegments + 1;
		else
			m_PartialSegments = m_PartialSegments + 1;
	}

	//! Reuse an existing vertex whose endpoint is within DM_ROAD_ENDPOINT_SNAP of
	//! pos (XZ only, so float noise in Y of coincident endpoints doesn't split a
	//! junction), else create a new one.
	private int FindOrCreateNode(vector pos, string kind)
	{
		int i;
		for (i = 0; i < m_Graph.Nodes.Count(); i++)
		{
			if (EndpointClose(m_Graph.Nodes[i].Pos, pos))
				return m_Graph.Nodes[i].Id;
		}
		return AddNode(pos, kind);
	}

	//! True when two endpoints are within DM_ROAD_ENDPOINT_SNAP in the XZ plane.
	private bool EndpointClose(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz <= DM_ROAD_ENDPOINT_SNAP * DM_ROAD_ENDPOINT_SNAP;
	}

	//! Create a new vertex.
	private int AddNode(vector pos, string kind)
	{
		dmRoadGraphNode node = new dmRoadGraphNode();
		node.Id = m_NextNodeId;
		node.Pos = pos;
		node.Kind = kind;
		m_Graph.Nodes.Insert(node);
		m_NextNodeId = m_NextNodeId + 1;
		return node.Id;
	}

	//! Create an edge between two vertices (the two endpoints of a segment).
	private void AddEdge(int fromNode, int toNode, vector endA, vector endB)
	{
		if (HasEdge(fromNode, toNode))
			return;

		float midX = (endA[0] + endB[0]) * 0.5;
		float midZ = (endA[2] + endB[2]) * 0.5;

		dmRoadGraphEdge edge = new dmRoadGraphEdge();
		edge.Id = m_NextEdgeId;
		edge.From = fromNode;
		edge.To = toNode;
		edge.Points = new array<vector>();
		edge.Points.Insert(endA);
		edge.Points.Insert(endB);
		edge.Length = vector.Distance(endA, endB);
		edge.SurfaceCategory = dmRoadSensor.CategoryAt(midX, midZ);
		edge.Obstacles = new array<ref dmRoadObstacle>();
		m_Graph.Edges.Insert(edge);
		m_NextEdgeId = m_NextEdgeId + 1;
	}

	//! True when an edge already connects a and b in either direction.
	private bool HasEdge(int a, int b)
	{
		int i;
		for (i = 0; i < m_Graph.Edges.Count(); i++)
		{
			if ((m_Graph.Edges[i].From == a && m_Graph.Edges[i].To == b) || (m_Graph.Edges[i].From == b && m_Graph.Edges[i].To == a))
				return true;
		}
		return false;
	}

	//! Connect hanging edges: merge deadends that point at each other across a
	//! gap (Pass A) and split edges at T-junctions where a deadend meets the
	//! middle of another road (Pass B). ClassifyNodes re-runs afterwards.
	private void ConnectGraph()
	{
		int merged = MergeDeadends();
		int split = SplitEdges();
		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[ROADNET] connect: merged=" + merged + " split=" + split);
		#endif
	}

	//! Number of edges incident to a vertex.
	private int Degree(int id)
	{
		int deg = 0;
		int i;
		for (i = 0; i < m_Graph.Edges.Count(); i++)
		{
			if (m_Graph.Edges[i].From == id || m_Graph.Edges[i].To == id)
				deg = deg + 1;
		}
		return deg;
	}

	//! Position of a vertex by id (scan; used during the connection passes).
	private vector NodePos(int id)
	{
		int i;
		for (i = 0; i < m_Graph.Nodes.Count(); i++)
		{
			if (m_Graph.Nodes[i].Id == id)
				return m_Graph.Nodes[i].Pos;
		}
		return vector.Zero;
	}

	//! Outward direction (XZ) from a deadend vertex toward the far end of its
	//! single incident edge. Returns false when the vertex has no incident edge.
	private bool DeadendOutward(int id, out vector dir)
	{
		int i;
		int farId;
		vector farPos;
		vector pos;
		for (i = 0; i < m_Graph.Edges.Count(); i++)
		{
			farId = -1;
			if (m_Graph.Edges[i].From == id)
				farId = m_Graph.Edges[i].To;
			else if (m_Graph.Edges[i].To == id)
				farId = m_Graph.Edges[i].From;
			if (farId < 0)
				continue;
			farPos = NodePos(farId);
			pos = NodePos(id);
			dir = farPos - pos;
			return true;
		}
		return false;
	}

	//! True when two points are within dist in the XZ plane.
	private bool WithinXZ(vector a, vector b, float dist)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz <= dist * dist;
	}

	//! True when two deadends point at each other across a gap (each outward
	//! direction points toward the other vertex).
	private bool AimAtEachOther(int i, int j)
	{
		vector dirI;
		vector dirJ;
		vector toJ;
		vector toI;
		if (!DeadendOutward(m_Graph.Nodes[i].Id, dirI))
			return false;
		if (!DeadendOutward(m_Graph.Nodes[j].Id, dirJ))
			return false;
		toJ = m_Graph.Nodes[j].Pos - m_Graph.Nodes[i].Pos;
		toI = m_Graph.Nodes[i].Pos - m_Graph.Nodes[j].Pos;
		if (dmRoadProbe.AngleDeg(dirI, toJ) >= DM_ROAD_CONNECT_ANGLE)
			return false;
		if (dmRoadProbe.AngleDeg(dirJ, toI) >= DM_ROAD_CONNECT_ANGLE)
			return false;
		return true;
	}

	//! Merge the vertex at removedIdx into keepIdx: rewrite its incident edges
	//! to point at keepIdx, then drop the vertex.
	private void MergeNode(int removedIdx, int keepIdx)
	{
		int removedId = m_Graph.Nodes[removedIdx].Id;
		int keepId = m_Graph.Nodes[keepIdx].Id;
		int e;
		for (e = 0; e < m_Graph.Edges.Count(); e++)
		{
			if (m_Graph.Edges[e].From == removedId)
				m_Graph.Edges[e].From = keepId;
			if (m_Graph.Edges[e].To == removedId)
				m_Graph.Edges[e].To = keepId;
		}
		m_Graph.Nodes.Remove(removedIdx);
	}

	//! Pass A: merge pairs of close deadends that point at each other (a road
	//! broken in two by the scan). Returns the number of merges.
	private int MergeDeadends()
	{
		int merged = 0;
		int i;
		int j;
		for (i = 0; i < m_Graph.Nodes.Count(); i++)
		{
			if (Degree(m_Graph.Nodes[i].Id) != 1)
				continue;
			for (j = i + 1; j < m_Graph.Nodes.Count(); j++)
			{
				if (Degree(m_Graph.Nodes[j].Id) != 1)
					continue;
				if (!WithinXZ(m_Graph.Nodes[i].Pos, m_Graph.Nodes[j].Pos, DM_ROAD_CONNECT_DIST))
					continue;
				if (!AimAtEachOther(i, j))
					continue;
				MergeNode(j, i);
				merged = merged + 1;
				break;
			}
		}
		return merged;
	}

	//! Rewrite the deadend vertex's incident edge so its P-end becomes Q, then
	//! drop the now-orphaned vertex P from the node list.
	private void ReconnectDeadendEnd(int pId, int qId)
	{
		int i;
		for (i = 0; i < m_Graph.Edges.Count(); i++)
		{
			if (m_Graph.Edges[i].From == pId)
				m_Graph.Edges[i].From = qId;
			if (m_Graph.Edges[i].To == pId)
				m_Graph.Edges[i].To = qId;
		}
		for (i = 0; i < m_Graph.Nodes.Count(); i++)
		{
			if (m_Graph.Nodes[i].Id == pId)
			{
				m_Graph.Nodes.Remove(i);
				break;
			}
		}
	}

	//! Pass B helper: split the edge at eIdx where the deadend P projects onto
	//! its interior, then reconnect P's own edge to the new split vertex Q.
	//! Returns true when a split happened.
	private bool TrySplitAtDeadend(int pId, vector pPos, int eIdx)
	{
		int fromId = m_Graph.Edges[eIdx].From;
		int toId = m_Graph.Edges[eIdx].To;
		vector aPos = NodePos(fromId);
		vector bPos = NodePos(toId);

		vector ab = bPos - aPos;
		float abLenSq = ab[0] * ab[0] + ab[2] * ab[2];
		if (abLenSq <= 0.0)
			return false;

		vector pa = pPos - aPos;
		float t = (pa[0] * ab[0] + pa[2] * ab[2]) / abLenSq;
		if (t <= 0.05 || t >= 0.95)
			return false;

		float qx = aPos[0] + ab[0] * t;
		float qz = aPos[2] + ab[2] * t;
		float dx = pPos[0] - qx;
		float dz = pPos[2] - qz;
		if (dx * dx + dz * dz >= DM_ROAD_CONNECT_DIST * DM_ROAD_CONNECT_DIST)
			return false;

		vector qPos = Vector(qx, GetGame().SurfaceY(qx, qz), qz);
		int qId = AddNode(qPos, "bend");

		//! Split edge e (A→B) into A→Q (in place) and Q→B (new edge).
		m_Graph.Edges[eIdx].To = qId;
		m_Graph.Edges[eIdx].Points = new array<vector>();
		m_Graph.Edges[eIdx].Points.Insert(aPos);
		m_Graph.Edges[eIdx].Points.Insert(qPos);
		m_Graph.Edges[eIdx].Length = vector.Distance(aPos, qPos);
		m_Graph.Edges[eIdx].SurfaceCategory = dmRoadSensor.CategoryAt((aPos[0] + qPos[0]) * 0.5, (aPos[2] + qPos[2]) * 0.5);
		m_Graph.Edges[eIdx].Obstacles = new array<ref dmRoadObstacle>();

		dmRoadGraphEdge e2 = new dmRoadGraphEdge();
		e2.Id = m_NextEdgeId;
		e2.From = qId;
		e2.To = toId;
		e2.Points = new array<vector>();
		e2.Points.Insert(qPos);
		e2.Points.Insert(bPos);
		e2.Length = vector.Distance(qPos, bPos);
		e2.SurfaceCategory = dmRoadSensor.CategoryAt((qPos[0] + bPos[0]) * 0.5, (qPos[2] + bPos[2]) * 0.5);
		e2.Obstacles = new array<ref dmRoadObstacle>();
		m_Graph.Edges.Insert(e2);
		m_NextEdgeId = m_NextEdgeId + 1;

		ReconnectDeadendEnd(pId, qId);
		return true;
	}

	//! Pass B: split edges at T-junctions where a deadend meets the middle of
	//! another road. Returns the number of splits.
	private int SplitEdges()
	{
		int split = 0;
		int ni;
		int pId;
		vector pPos;
		int edgeCount;
		int ei;
		for (ni = 0; ni < m_Graph.Nodes.Count(); ni++)
		{
			pId = m_Graph.Nodes[ni].Id;
			if (Degree(pId) != 1)
				continue;
			pPos = m_Graph.Nodes[ni].Pos;
			edgeCount = m_Graph.Edges.Count();
			for (ei = 0; ei < edgeCount; ei++)
			{
				if (m_Graph.Edges[ei].From == pId || m_Graph.Edges[ei].To == pId)
					continue;
				if (!TrySplitAtDeadend(pId, pPos, ei))
					continue;
				split = split + 1;
				break;
			}
		}
		return split;
	}

	//! Fill Rise/Fall/AvgFriction/SurfaceCategory/Obstacles for every edge.
	private void AccumulateMetadata()
	{
		int ei;
		for (ei = 0; ei < m_Graph.Edges.Count(); ei++)
			AccumulateEdge(ei);
	}

	//! Dominant mnemonic category from the four category counters (first wins ties).
	private string DominantCategory(int paved, int dirt, int gravel, int unknown)
	{
		int best = paved;
		string cat = "paved";
		if (dirt > best)
		{
			best = dirt;
			cat = "dirt";
		}
		if (gravel > best)
		{
			best = gravel;
			cat = "gravel";
		}
		if (unknown > best)
		{
			best = unknown;
			cat = "unknown";
		}
		return cat;
	}

	//! Walk one edge from A to B in DM_ROAD_META_STEP samples, accumulating the
	//! vertical rise/fall, the average friction, the dominant surface category and
	//! the deduplicated obstacle list.
	private void AccumulateEdge(int eIdx)
	{
		vector aPos = NodePos(m_Graph.Edges[eIdx].From);
		vector bPos = NodePos(m_Graph.Edges[eIdx].To);
		float len = vector.Distance(aPos, bPos);
		int n = Math.Floor(len / DM_ROAD_META_STEP) + 1;
		float invN = 1.0 / n;

		float rise = 0.0;
		float fall = 0.0;
		float frictionSum = 0.0;
		int paved = 0;
		int dirt = 0;
		int gravel = 0;
		int unknown = 0;

		ref map<string, bool> seenObs = new map<string, bool>();
		array<Object> objs = new array<Object>();
		array<CargoBase> cargos = new array<CargoBase>();
		vector mm[2];

		float prevY = 0.0;
		bool havePrev = false;

		int k;
		int i;
		Object obj;
		string type;
		string key;
		string name;
		string cat;
		float friction;
		int cls;
		vector point;
		vector op;
		float t;
		float y;
		float dy;
		float radius;
		dmRoadObstacle obs;

		m_Graph.Edges[eIdx].Obstacles = new array<ref dmRoadObstacle>();

		for (k = 0; k <= n; k++)
		{
			t = k * invN;
			point = aPos + (bPos - aPos) * t;
			y = GetGame().SurfaceY(point[0], point[2]);

			if (havePrev)
			{
				dy = y - prevY;
				rise = rise + Math.Max(0.0, dy);
				fall = fall + Math.Max(0.0, -dy);
			}
			prevY = y;
			havePrev = true;

			dmRoadSensor.SampleSurface(point[0], point[2], name, friction, cls);
			frictionSum = frictionSum + friction;
			cat = dmRoadSensor.Category(name);
			if (cat == "paved")
				paved = paved + 1;
			else if (cat == "dirt")
				dirt = dirt + 1;
			else if (cat == "gravel")
				gravel = gravel + 1;
			else
				unknown = unknown + 1;

			objs.Clear();
			cargos.Clear();
			GetGame().GetObjectsAtPosition(Vector(point[0], y, point[2]), DM_ROAD_OBSTACLE_RADIUS, objs, cargos);
			for (i = 0; i < objs.Count(); i++)
			{
				obj = objs[i];
				if (dmRoadSensor.IsRoadObject(obj))
					continue;
				type = obj.GetType();
				type.ToLower();
				if (type.Contains("tree") || type.Contains("bush") || type.Contains("grass") || type.Contains("rock") || type.Contains("shrub"))
					continue;
				radius = obj.ClippingInfo(mm);
				if (radius <= 1.0)
					continue;
				op = obj.GetPosition();
				key = dmRoadProbe.CellKey(op[0], op[2], 3.0);
				if (seenObs.Contains(key))
					continue;
				seenObs.Insert(key, true);
				obs = new dmRoadObstacle();
				obs.Pos = op;
				obs.Type = obj.GetType();
				m_Graph.Edges[eIdx].Obstacles.Insert(obs);
			}
		}

		m_Graph.Edges[eIdx].Rise = rise;
		m_Graph.Edges[eIdx].Fall = fall;
		m_Graph.Edges[eIdx].AvgFriction = frictionSum / n;
		m_Graph.Edges[eIdx].SurfaceCategory = DominantCategory(paved, dirt, gravel, unknown);
	}
}
