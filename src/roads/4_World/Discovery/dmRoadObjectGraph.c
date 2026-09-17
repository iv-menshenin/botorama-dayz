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

	void dmRoadObjectGraphBuilder()
	{
		m_Graph = null;
		m_Seen = new map<string, bool>();
		m_NextNodeId = 0;
		m_NextEdgeId = 0;
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
		dmBotLog.Debug("[ROADNET] objects=" + objects.Count() + " nodes=" + m_Graph.Nodes.Count() + " edges=" + m_Graph.Edges.Count());
		#endif

		return m_Graph;
	}

	//! Grid-scan [min,max] for road objects, dedup by position cell, and append
	//! each unique road object to outObjects.
	private void ScanObjects(vector min, vector max, array<Object> outObjects)
	{
		float x;
		float z;
		float terrainY;
		array<Object> objs;
		array<CargoBase> cargos;
		int i;
		Object obj;
		vector p;
		string key;

		for (x = min[0]; x <= max[0]; x = x + DM_ROAD_SCAN_STEP)
		{
			for (z = min[2]; z <= max[2]; z = z + DM_ROAD_SCAN_STEP)
			{
				terrainY = GetGame().SurfaceY(x, z);
				objs = new array<Object>();
				cargos = new array<CargoBase>();
				GetGame().GetObjectsAtPosition(Vector(x, terrainY, z), 12.0, objs, cargos);
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
	//! Objects with only one endpoint pair (a stub) are skipped.
	private void BuildSegment(Object obj)
	{
		bool hasA = obj.MemoryPointExists("LB") && obj.MemoryPointExists("PB");
		bool hasB = obj.MemoryPointExists("LE") && obj.MemoryPointExists("PE");

		vector endA;
		vector endB;
		bool haveA = false;
		bool haveB = false;

		if (hasA)
		{
			vector lb = obj.ModelToWorld(obj.GetMemoryPointPos("LB"));
			vector pb = obj.ModelToWorld(obj.GetMemoryPointPos("PB"));
			endA = (lb + pb) * 0.5;
			haveA = true;
		}
		if (hasB)
		{
			vector le = obj.ModelToWorld(obj.GetMemoryPointPos("LE"));
			vector pe = obj.ModelToWorld(obj.GetMemoryPointPos("PE"));
			endB = (le + pe) * 0.5;
			haveB = true;
		}

		if (!haveA || !haveB)
			return;

		int nodeA = FindOrCreateNode(endA, "junction");
		int nodeB = FindOrCreateNode(endB, "junction");
		AddEdge(nodeA, nodeB, endA, endB);
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
		edge.SurfaceType = dmRoadSensor.Classify(midX, midZ);
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
}
