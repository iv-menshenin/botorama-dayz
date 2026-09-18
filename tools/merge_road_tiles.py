#!/usr/bin/env python3
"""Merge road-discovery tiles into road_graph.json + road_gaps.json (offline).

Usage: merge_road_tiles.py <tiles_dir> <out_graph> <out_gaps> [world_name]

Mirrors the Enfusion dmRoadDiscoveryManager.Merge: combine tiles (renumber ids),
sew coincident vertices (<= DM_ROAD_ENDPOINT_SNAP in XZ), reclassify by degree,
detect gaps (deadends aiming at each other <= DM_ROAD_GAP_MAX / < DM_ROAD_GAP_ANGLE).
"""
import json, sys, glob, os, math, collections

tiles_dir = sys.argv[1]
out_graph = sys.argv[2]
out_gaps = sys.argv[3]
world_name = sys.argv[4] if len(sys.argv) > 4 else "chernarusplus"

SNAP = 2.0        # DM_ROAD_ENDPOINT_SNAP
GAP_MAX = 200.0   # DM_ROAD_GAP_MAX
GAP_ANGLE = 25.0  # DM_ROAD_GAP_ANGLE


def tile_key(path):
    base = os.path.basename(path)[:-5]  # strip ".json"
    tx, tz = base.split(":")
    return (int(tx), int(tz))


files = sorted(glob.glob(os.path.join(tiles_dir, "*.json")), key=tile_key)

nodes = []  # dict(Id, Pos, Kind)
edges = []  # dict(Id, From, To, Points, Length, SurfaceCategory, AvgFriction, Rise, Fall, Obstacles)

node_offset = 0
edge_offset = 0
for f in files:
    g = json.load(open(f))
    for n in g.get("Nodes", []):
        n = dict(n)
        n["Id"] += node_offset
        nodes.append(n)
    for e in g.get("Edges", []):
        e = dict(e)
        e["Id"] += edge_offset
        e["From"] += node_offset
        e["To"] += node_offset
        edges.append(e)
    node_offset += len(g.get("Nodes", []))
    edge_offset += len(g.get("Edges", []))

print(f"loaded {len(files)} tiles: {len(nodes)} nodes, {len(edges)} edges")

# --- Sew vertices within SNAP (XZ) via union-find + spatial hash ---
parent = list(range(len(nodes)))


def find(x):
    while parent[x] != x:
        parent[x] = parent[parent[x]]
        x = parent[x]
    return x


def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb:
        parent[rb] = ra  # merge into the earlier


cell = {}
for i, n in enumerate(nodes):
    cx = int(n["Pos"][0] // SNAP)
    cz = int(n["Pos"][2] // SNAP)
    cell.setdefault((cx, cz), []).append(i)

for i, n in enumerate(nodes):
    cx = int(n["Pos"][0] // SNAP)
    cz = int(n["Pos"][2] // SNAP)
    for dx in (-1, 0, 1):
        for dz in (-1, 0, 1):
            for j in cell.get((cx + dx, cz + dz), []):
                if j <= i:
                    continue
                if find(i) == find(j):
                    continue
                ddx = nodes[i]["Pos"][0] - nodes[j]["Pos"][0]
                ddz = nodes[i]["Pos"][2] - nodes[j]["Pos"][2]
                if ddx * ddx + ddz * ddz <= SNAP * SNAP:
                    union(i, j)

root_to_newid = {}
new_nodes = []
for i, n in enumerate(nodes):
    r = find(i)
    if r not in root_to_newid:
        root_to_newid[r] = len(new_nodes)
        nn = dict(n)
        nn["Id"] = len(new_nodes)  # renumber to 0..N-1 (edges use these indices)
        new_nodes.append(nn)

seen_edges = set()
new_edges = []
for e in edges:
    f = root_to_newid[find(e["From"])]
    t = root_to_newid[find(e["To"])]
    if f == t:
        continue  # self-loop
    key = (min(f, t), max(f, t))
    if key in seen_edges:
        continue  # duplicate
    seen_edges.add(key)
    e = dict(e)
    e["Id"] = len(new_edges)
    e["From"] = f
    e["To"] = t
    new_edges.append(e)

# --- Reclassify by degree ---
deg = collections.Counter()
for e in new_edges:
    deg[e["From"]] += 1
    deg[e["To"]] += 1

for i, n in enumerate(new_nodes):
    d = deg.get(i, 0)
    if d >= 3:
        n["Kind"] = "junction"
    elif d == 2:
        n["Kind"] = "bend"
    elif d == 1:
        n["Kind"] = "deadend"
    else:
        n["Kind"] = "isolated"


# --- Gaps: deadends aiming at each other ---
def outward(nid):
    n = new_nodes[nid]
    nx, nz = n["Pos"][0], n["Pos"][2]
    for e in new_edges:
        if e["From"] == nid:
            o = new_nodes[e["To"]]
        elif e["To"] == nid:
            o = new_nodes[e["From"]]
        else:
            continue
        dx, dz = o["Pos"][0] - nx, o["Pos"][2] - nz
        L = math.hypot(dx, dz) or 1.0
        return (dx / L, dz / L)
    return None


def angle(a, b):
    dot = a[0] * b[0] + a[1] * b[1]
    cross = a[0] * b[1] - a[1] * b[0]
    return math.degrees(math.atan2(abs(cross), dot))


deadends = [i for i in range(len(new_nodes)) if deg.get(i, 0) == 1]
gaps = []
gid = 0
for ai in range(len(deadends)):
    i = deadends[ai]
    di = outward(i)
    if not di:
        continue
    for bj in range(ai + 1, len(deadends)):
        j = deadends[bj]
        nx1, nz1 = new_nodes[i]["Pos"][0], new_nodes[i]["Pos"][2]
        nx2, nz2 = new_nodes[j]["Pos"][0], new_nodes[j]["Pos"][2]
        dist = math.hypot(nx2 - nx1, nz2 - nz1)
        if dist > GAP_MAX:
            continue
        vx, vz = (nx2 - nx1) / dist, (nz2 - nz1) / dist
        a1 = angle(di, (vx, vz))
        dj = outward(j)
        if not dj:
            continue
        a2 = angle(dj, (-vx, -vz))
        if a1 < GAP_ANGLE and a2 < GAP_ANGLE:
            gaps.append({
                "Id": gid,
                "FromNode": i, "FromPos": new_nodes[i]["Pos"],
                "ToNode": j, "ToPos": new_nodes[j]["Pos"],
                "Distance": dist, "AngleA": a1, "AngleB": a2, "Status": "pending",
            })
            gid += 1

json.dump({"Nodes": new_nodes, "Edges": new_edges}, open(out_graph, "w"))
json.dump({"Version": 1, "WorldName": world_name, "Gaps": gaps}, open(out_gaps, "w"), indent=2)

print(f"merged: {len(new_nodes)} nodes, {len(new_edges)} edges, {len(gaps)} gaps "
      f"(junction={sum(1 for n in new_nodes if n['Kind']=='junction')}, "
      f"deadend={sum(1 for n in new_nodes if n['Kind']=='deadend')})")
