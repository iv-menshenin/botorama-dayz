#!/usr/bin/env python3
"""Simplify the full road graph into a coarse settlement/junction skeleton.

Usage: simplify_road_graph.py <road_graph.json> <simplified_graph.json> <out.png> [world_name]

Deterministic derivation of road_graph.json (the full graph is the source of
truth). The simplified graph inherits the full graph's identification via
provenance fields:

- Node.Members  : full-graph node Ids absorbed by this vertex (a settlement
                  lists all its member junctions; a lone junction lists itself).
- Edge.Path     : ordered full-graph node Ids along the contracted degree-2 chain
                  (source member -> bends -> target member).

Contraction rules:
- settlements : spatial clusters of junctions (degree>=3) within cluster_radius,
                with >= minJunctions members -> one 'settlement' vertex
                (centroid + Weight + Radius).
- lone junctions : clusters of size 1 -> one 'junction' vertex.
- bends (degree 2) : contracted into edges (total length, dominant
                SurfaceCategory by length, length-weighted AvgFriction).
- deadends + their spurs : dropped (option A).
"""
import json, sys, math, collections

graph_path = sys.argv[1]
out_json = sys.argv[2]
out_png = sys.argv[3]
world_name = sys.argv[4] if len(sys.argv) > 4 else "chernarusplus"

CLUSTER_R = 150.0
MIN_JUNCTIONS = 2

g = json.load(open(graph_path))
nodes = g["Nodes"]
edges = g["Edges"]

deg = collections.Counter()
for e in edges:
    deg[e["From"]] += 1
    deg[e["To"]] += 1

deadends = set(n["Id"] for n in nodes if deg.get(n["Id"], 0) == 1)
junctions = [n for n in nodes if deg.get(n["Id"], 0) >= 3]


def cluster(points, R):
    """Union-find spatial clustering of points by XZ proximity radius R."""
    R2 = R * R
    n = len(points)
    par = list(range(n))

    def find(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            par[rb] = ra

    cell = int(R) + 1
    grid = {}
    for i, p in enumerate(points):
        k = (int(p["Pos"][0] // cell), int(p["Pos"][2] // cell))
        grid.setdefault(k, []).append(i)
    for i, p in enumerate(points):
        cx, cz = int(p["Pos"][0] // cell), int(p["Pos"][2] // cell)
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                for j in grid.get((cx + dx, cz + dz), []):
                    if j <= i or find(i) == find(j):
                        continue
                    a = points[i]["Pos"]
                    b = points[j]["Pos"]
                    if (a[0] - b[0]) ** 2 + (a[2] - b[2]) ** 2 <= R2:
                        union(i, j)
    comp = collections.defaultdict(list)
    for i in range(n):
        comp[find(i)].append(i)
    return comp


# ---- coarse vertices: settlements + lone junctions ----
coarse_nodes = []       # dicts: Id, Type, Pos, [Weight, Radius], Members
node_anchor = {}        # full-graph node Id -> coarse vertex Id (junctions only)

jcomp = cluster(junctions, CLUSTER_R)
for members_idx in jcomp.values():
    members = [junctions[i] for i in members_idx]
    if len(members) >= MIN_JUNCTIONS:
        xs = [m["Pos"][0] for m in members]
        ys = [m["Pos"][1] for m in members]
        zs = [m["Pos"][2] for m in members]
        cx = sum(xs) / len(xs)
        cy = sum(ys) / len(ys)
        cz = sum(zs) / len(zs)
        rad = max(math.hypot(m["Pos"][0] - cx, m["Pos"][2] - cz) for m in members)
        cid = len(coarse_nodes)
        coarse_nodes.append({
            "Id": cid, "Type": "settlement",
            "Pos": [round(cx, 3), round(cy, 3), round(cz, 3)],
            "Weight": len(members),
            "Radius": round(rad, 3),
            "Members": sorted(m["Id"] for m in members),
        })
        for m in members:
            node_anchor[m["Id"]] = cid
    else:
        m = members[0]
        cid = len(coarse_nodes)
        coarse_nodes.append({
            "Id": cid, "Type": "junction",
            "Pos": [round(m["Pos"][0], 3), round(m["Pos"][1], 3), round(m["Pos"][2], 3)],
            "Members": [m["Id"]],
        })
        node_anchor[m["Id"]] = cid

# ---- adjacency (edge index -> neighbor) ----
adj = collections.defaultdict(list)
for i, e in enumerate(edges):
    adj[e["From"]].append((i, e["To"]))
    adj[e["To"]].append((i, e["From"]))

consumed = set()  # edge indices already walked


def walk_chain(start_nid, eidx):
    """Follow a degree-2 chain from terminal node start_nid via edge eidx.

    Returns (end_nid, length, path, surf_counter, fric_weighted_sum).
    end_nid is a terminal (anchor or deadend) or the last node of a malformed
    chain.
    """
    path = [start_nid]
    total = 0.0
    surf = collections.Counter()
    fric_wsum = 0.0
    cur = start_nid
    cur_e = eidx
    while True:
        consumed.add(cur_e)
        e = edges[cur_e]
        ln = e.get("Length", 0) or 0.0
        total += ln
        surf[e.get("SurfaceCategory", "unknown")] += ln
        fric_wsum += (e.get("AvgFriction", 0) or 0.0) * ln
        nxt = e["From"] if e["To"] == cur else e["To"]
        path.append(nxt)
        if nxt in node_anchor or nxt in deadends:
            return nxt, total, path, surf, fric_wsum
        # nxt is a bend (degree 2): continue through its other edge
        nxt_edges = [ei for ei, nn in adj[nxt] if ei != cur_e]
        if not nxt_edges:
            return nxt, total, path, surf, fric_wsum
        cur = nxt
        cur_e = nxt_edges[0]


coarse_edges = []


def add_edge(a_from, a_to, length, path, surf, fric_wsum):
    if a_from == a_to or length <= 0:
        return
    dom = surf.most_common(1)[0][0] if surf else "unknown"
    avg_fric = fric_wsum / length if length > 0 else 0.0
    coarse_edges.append({
        "From": a_from, "To": a_to,
        "Length": round(length, 3),
        "SurfaceCategory": dom,
        "AvgFriction": round(avg_fric, 3),
        "Path": path,
    })


for nid in list(node_anchor.keys()):
    a_from = node_anchor[nid]
    for (eidx, nb) in adj[nid]:
        if eidx in consumed:
            continue
        if nb in node_anchor:
            # direct edge between two anchored nodes
            consumed.add(eidx)
            e = edges[eidx]
            ln = e.get("Length", 0) or 0.0
            add_edge(a_from, node_anchor[nb], ln, [nid, nb],
                     collections.Counter({e.get("SurfaceCategory", "unknown"): ln}),
                     (e.get("AvgFriction", 0) or 0.0) * ln)
        elif nb in deadends:
            consumed.add(eidx)  # spur to deadend -> dropped
        else:
            end, ln, path, surf, fric = walk_chain(nid, eidx)
            if end in node_anchor:
                add_edge(a_from, node_anchor[end], ln, path, surf, fric)
            # end in deadends -> dropped

result = {
    "Version": 1,
    "WorldName": world_name,
    "SourceGraph": graph_path.rsplit("/", 1)[-1],
    "Parameters": {"clusterRadius": CLUSTER_R, "minJunctions": MIN_JUNCTIONS},
    "Nodes": coarse_nodes,
    "Edges": coarse_edges,
}

json.dump(result, open(out_json, "w"))

n_settle = sum(1 for n in coarse_nodes if n["Type"] == "settlement")
n_junc = sum(1 for n in coarse_nodes if n["Type"] == "junction")
print("simplified: %d nodes (%d settlements + %d junctions), %d edges"
      % (len(coarse_nodes), n_settle, n_junc, len(coarse_edges)))
print("wrote %s" % out_json)

# ---- render ----
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

surf_color = {"paved": "#1a1a1a", "dirt": "#8b5a2b", "gravel": "#c2a860",
              "unknown": "#999999"}

fig, ax = plt.subplots(figsize=(28, 28), dpi=130)

# faint full graph background (nodes indexed by Id -> build a map)
node_map = {n["Id"]: n for n in nodes}
for e in edges:
    a = node_map.get(e["From"])
    b = node_map.get(e["To"])
    if not a or not b:
        continue
    ax.plot([a["Pos"][0], b["Pos"][0]], [a["Pos"][2], b["Pos"][2]],
            color="#cccccc", lw=0.3, alpha=0.25, solid_capstyle="round", zorder=1)

# coarse edges
for e in coarse_edges:
    a = coarse_nodes[e["From"]]
    b = coarse_nodes[e["To"]]
    c = surf_color.get(e.get("SurfaceCategory", "unknown"), "#999")
    ax.plot([a["Pos"][0], b["Pos"][0]], [a["Pos"][2], b["Pos"][2]],
            color=c, lw=1.4, alpha=0.9, solid_capstyle="round", zorder=3)

# settlements (big) + junctions (small)
sx, sz, sw = [], [], []
jx, jz = [], []
for n in coarse_nodes:
    if n["Type"] == "settlement":
        sx.append(n["Pos"][0]); sz.append(n["Pos"][2]); sw.append(n["Weight"])
    else:
        jx.append(n["Pos"][0]); jz.append(n["Pos"][2])
if sx:
    ax.scatter(sx, sz, s=[10 + w * 2.0 for w in sw], c="red", zorder=5,
               edgecolors="darkred", linewidths=0.5, label="settlement")
if jx:
    ax.scatter(jx, jz, s=12, c="blue", zorder=5, label="junction")

ax.set_aspect("equal")
ax.set_xlabel("X (east)")
ax.set_ylabel("Z (north)")
ax.set_title("simplified road graph — %d settlements, %d junctions, %d edges"
             % (n_settle, n_junc, len(coarse_edges)))
ax.legend(loc="upper right", markerscale=0.5)
ax.grid(True, alpha=0.2)
fig.tight_layout()
fig.savefig(out_png)
print("wrote %s" % out_png)
