#!/usr/bin/env python3
"""Render the whole-map road graph + gaps to PNG.

Usage: render_road_map.py <road_graph.json> <road_gaps.json> <out.png>
"""
import json, sys, collections, math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

graph = json.load(open(sys.argv[1]))
gaps = json.load(open(sys.argv[2])) if len(sys.argv) > 2 and sys.argv[2] else None
out = sys.argv[3]

nodes = {n["Id"]: n for n in graph["Nodes"]}
edges = graph["Edges"]

deg = collections.Counter()
for e in edges:
    deg[e["From"]] += 1
    deg[e["To"]] += 1

surf_color = {
    "paved": "#1a1a1a",
    "dirt": "#8b5a2b",
    "gravel": "#c2a860",
    "unknown": "#999999",
}

fig, ax = plt.subplots(figsize=(28, 28), dpi=130)

for e in edges:
    a = nodes.get(e["From"])
    b = nodes.get(e["To"])
    if not a or not b:
        continue
    c = surf_color.get(e.get("SurfaceCategory", "unknown"), "#999")
    ax.plot([a["Pos"][0], b["Pos"][0]], [a["Pos"][2], b["Pos"][2]],
            color=c, lw=0.7, alpha=0.8, solid_capstyle="round")

# junctions (deg>=3) red, deadends (deg==1) black
jx, jz, dx, dz = [], [], [], []
for nid, n in nodes.items():
    d = deg.get(nid, 0)
    if d >= 3:
        jx.append(n["Pos"][0]); jz.append(n["Pos"][2])
    elif d == 1:
        dx.append(n["Pos"][0]); dz.append(n["Pos"][2])
ax.scatter(jx, jz, s=4, c="red", zorder=6)
ax.scatter(dx, dz, s=2, c="black", zorder=6)

# gaps circled (radius ~60 m around the gap midpoint)
if gaps:
    for g in gaps.get("Gaps", []):
        a = g.get("FromPos"); b = g.get("ToPos")
        if not a or not b:
            continue
        mx = (a[0] + b[0]) * 0.5
        mz = (a[2] + b[2]) * 0.5
        ax.add_patch(plt.Circle((mx, mz), 60.0, fill=False,
                                edgecolor="red", lw=1.5, zorder=7))

ax.set_aspect("equal")
ax.set_xlabel("X (east)")
ax.set_ylabel("Z (north)")
ax.set_title(f"road graph — {len(nodes)} nodes, {len(edges)} edges; "
             f"red=junction, black=deadend, red circle=gap"
             + (f" ({len(gaps.get('Gaps', []))} gaps)" if gaps else ""))
ax.grid(True, alpha=0.2)
fig.tight_layout()
fig.savefig(out)
print(f"saved {out}: {len(nodes)} nodes, {len(edges)} edges, "
      f"{sum(1 for d in deg.values() if d >= 3)} junctions, "
      f"{sum(1 for d in deg.values() if d == 1)} deadends")
