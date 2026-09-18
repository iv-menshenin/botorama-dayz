#!/usr/bin/env python3
"""Render the road graph (nodes + edges) to PNG with degree-aware styling.

Usage: render_road_graph.py <road_graph.json> <out.png>
- edges: colored by surface type (thick lines)
- nodes: degree>=3 = red (junction), degree==1 = black (deadend),
         degree==0 = orange (isolated); degree==2 (bend) not drawn
- edge length labeled at midpoint
"""
import json, sys, collections
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

path = sys.argv[1]
out = sys.argv[2]
g = json.load(open(path))

nodes = {n["Id"]: n for n in g["Nodes"]}
edges = g["Edges"]

# degree per node
deg = collections.Counter()
for e in edges:
    deg[e["From"]] += 1
    deg[e["To"]] += 1

surf_color = {
    0: "#999999",  # unknown
    1: "#111111",  # paved (asphalt/concrete)
    2: "#8b5a2b",  # dirt/gravel
    3: "#2e8b57",  # grass
    4: "#1b5e20",  # forest
    5: "#7b1fa2",  # structure
    6: "#1565c0",  # water
}

fig, ax = plt.subplots(figsize=(20, 18), dpi=130)

# edges (thick lines) + length labels
for e in edges:
    a = nodes.get(e["From"])
    b = nodes.get(e["To"])
    if not a or not b:
        continue
    xs = [a["Pos"][0], b["Pos"][0]]
    zs = [a["Pos"][2], b["Pos"][2]]
    c = surf_color.get(e.get("SurfaceType", 0), "#999")
    ax.plot(xs, zs, color=c, lw=2.2, alpha=0.85, solid_capstyle="round")
    # length label at midpoint (small)
    mx = (xs[0] + xs[1]) * 0.5
    mz = (zs[0] + zs[1]) * 0.5
    ax.text(mx, mz, f"{e.get('Length', 0):.0f}", fontsize=4.5, color="#333333",
            ha="center", va="center", alpha=0.55)

# nodes: only degree != 2
for nid, n in nodes.items():
    d = deg.get(nid, 0)
    x, z = n["Pos"][0], n["Pos"][2]
    if d >= 3:
        ax.plot(x, z, "o", color="red", ms=5, mew=0.5, mec="white", zorder=6)
    elif d == 1:
        ax.plot(x, z, "s", color="black", ms=5, zorder=6)
    elif d == 0:
        ax.plot(x, z, "D", color="orange", ms=5, zorder=6)
    # d == 2: bend — skip

ax.set_aspect("equal")
ax.set_xlabel("X (east)")
ax.set_ylabel("Z (north)")
ax.set_title(
    "road graph — edges colored by surface (dark=asphalt, brown=dirt); "
    "red=junction, black=deadend, orange=isolated; numbers = edge length (m)"
)
ax.grid(True, alpha=0.25)
fig.tight_layout()
fig.savefig(out)
print(f"saved {out}: {len(nodes)} nodes, {len(edges)} edges, "
      f"{sum(1 for d in deg.values() if d >= 3)} junctions, "
      f"{sum(1 for d in deg.values() if d == 1)} deadends")
