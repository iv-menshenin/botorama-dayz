//! dmSimplifiedGraph — coarse settlement/junction skeleton of the road network.
//!
//! Deserialized from simplified_graph.json (offline, deterministic derivation of
//! the full road graph — see tools/simplify_road_graph.py). Vertices are
//! settlements (clusters of >=2 junctions) and lone junctions; edges are
//! contracted degree-2 chains. Provenance ties every element back to the full
//! graph: Node.Members (full-graph node Ids absorbed by a vertex) and Edge.Path
//! (ordered full-graph node Ids along the contracted chain).

//! One coarse vertex: a settlement cluster or a lone junction.
class dmSimplifiedNode
{
	int Id;
	string Type;
	vector Pos;
	int Weight;
	float Radius;
	ref array<int> Members;
}

//! One coarse edge: a contracted degree-2 chain between two coarse vertices.
class dmSimplifiedEdge
{
	int From;
	int To;
	float Length;
	string SurfaceCategory;
	float AvgFriction;
	ref array<int> Path;
}

//! The coarse graph (root of simplified_graph.json).
class dmSimplifiedGraph
{
	ref array<ref dmSimplifiedNode> Nodes;
	ref array<ref dmSimplifiedEdge> Edges;
}
