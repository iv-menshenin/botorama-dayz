#!/usr/bin/env python3
"""render_honeycomb.py — render a dmRoadHoneycomb grid dump to an SVG.

Draws everything in the honeycomb's doubled frame (row along the road -> +X axis,
col across -> +Y axis), in meters. World points (route, road edges, objects) are
projected into that frame through the territory frame (origin/dir/side) from
the dump:

    along  = (p - origin) . dir
    across = (p - origin) . side

Cell centers come straight from the dump's world X/Z (CellCenter(col,row)); each is
drawn as a pointy-top hexagon of circumradius HEX_SIZE (0.5 m) so adjacent
hexagons tile edge-to-edge and match the dump centers.

Input 1 — griddump.json (produced by the E2E `griddump` op):

    {
      "Origin":    [x, y, z],
      "Dir":       [dx, dy, dz],
      "Side":      [sx, sy, sz],
      "Rows":      393,
      "Width":     181,
      "HalfWidth": 90,
      "Status":    "done",
      "CellCount": 35763,
      "Cells":     [ { "Row": 0, "Col": -90, "Color": 2, "X": .., "Z": .. }, ... ],
      "Route":     [ [x, y, z], ... ],
      "LeftEdge":  [ [x, y, z], ... ],
      "RightEdge": [ [x, y, z], ... ]
    }

    Cell "Color": 0 = unset (white), 1 = RED, 2 = GREEN, 3 = SAFE (dark green).
    Cell "Row"/"Col" are the doubled coords (row along, col across); "X"/"Z" are
    the world center of the hexagon. Odd (col+row) slots are skipped by the dump.

Input 2 — scanbox.json (objects from the E2E `scanbox` op; the op dumps one
line per entity as "ent[D|S] <ClassName> pos=(x,y,z)" — the tester converts
those lines into):

    {
      "objects": [
        { "ClassName": "Land_Obstacle_...", "Pos": [x, y, z] },
        ...
      ]
    }

    A missing or empty scanbox.json is fine (objects are simply skipped).

Output: an SVG file (".svg" is appended to the output path if absent).

Usage:
    python3 tools/render_honeycomb.py griddump.json scanbox.json out.svg
"""

import json
import math
import os
import sys

# Circumradius of a honeycomb hexagon (m) — must match DM_GRID_HEX_SIZE in
# dmRoadHoneycomb.c (0.5).
HEX_SIZE = 0.5

# Cell color -> SVG fill.
COLOR_FILL = {
    0: "#ffffff",  # unset
    1: "#e05555",  # RED
    2: "#4caf50",  # GREEN
    3: "#2e7d32",  # SAFE (dark green)
}


def as_float3(v):
    return [float(v[0]), float(v[1]), float(v[2])]


def project(p, origin, dirv, sidev):
    dx = p[0] - origin[0]
    dz = p[2] - origin[2]
    along = dx * dirv[0] + dz * dirv[2]
    across = dx * sidev[0] + dz * sidev[2]
    return along, across


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2

    grid_path, scan_path, out_path = argv[1], argv[2], argv[3]

    with open(grid_path, "r", encoding="utf-8") as fh:
        grid = json.load(fh)

    origin = as_float3(grid["Origin"])
    dirv = as_float3(grid["Dir"])
    sidev = as_float3(grid["Side"])
    rows = int(grid["Rows"])
    status = grid.get("Status", "")

    cells = grid.get("Cells", [])
    route = grid.get("Route", [])
    left_edge = grid.get("LeftEdge", [])
    right_edge = grid.get("RightEdge", [])

    objects = []
    if os.path.exists(scan_path):
        with open(scan_path, "r", encoding="utf-8") as fh:
            scan = json.load(fh)
        raw_objs = scan.get("objects", []) if isinstance(scan, dict) else scan
        for obj in raw_objs:
            pos = obj.get("Pos", None) if isinstance(obj, dict) else obj
            if not isinstance(pos, list) or len(pos) < 3:
                continue
            name = "?"
            if isinstance(obj, dict):
                name = str(obj.get("ClassName", "?"))
            objects.append((name, as_float3(pos)))

    # Project cell centers (dump world X/Z) and world features into the territory
    # frame (meters); bounds are derived from them so hexes always fit.
    cell_pts = []
    for cell in cells:
        gx, gy = project((float(cell["X"]), 0.0, float(cell["Z"])), origin, dirv, sidev)
        cell_pts.append((gx, gy))

    def feature_pts(points):
        return [project(as_float3(p), origin, dirv, sidev) for p in points]

    all_pts = list(cell_pts)
    all_pts += feature_pts(route)
    all_pts += feature_pts(left_edge)
    all_pts += feature_pts(right_edge)

    pad = HEX_SIZE + 1.0
    if all_pts:
        min_x = min(p[0] for p in all_pts) - pad
        max_x = max(p[0] for p in all_pts) + pad
        min_y = min(p[1] for p in all_pts) - pad
        max_y = max(p[1] for p in all_pts) + pad
    else:
        min_x = -1.0
        max_x = 1.0
        min_y = -1.0
        max_y = 1.0

    scale = 8.0
    margin = 80.0

    def sx(gx):
        return margin + (gx - min_x) * scale

    def sy(gy):
        return margin + (max_y - gy) * scale

    view_w = margin * 2.0 + (max_x - min_x) * scale
    view_h = margin * 2.0 + (max_y - min_y) * scale

    parts = []
    parts.append('<?xml version="1.0" encoding="UTF-8"?>')
    parts.append(
        '<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{1}" '
        'viewBox="0 0 {0} {1}">'.format(view_w, view_h)
    )
    parts.append(
        '<rect x="0" y="0" width="{0}" height="{1}" fill="#ffffff"/>'.format(view_w, view_h)
    )

    # Cells as pointy-top hexagons (honeycomb), vertices edge-to-edge with the
    # dump centers; thin black outline.
    HEX_R = HEX_SIZE
    for idx, cell in enumerate(cells):
        color = int(cell["Color"])
        fill = COLOR_FILL.get(color, "#ffffff")
        gx, gy = cell_pts[idx]
        pts = []
        for k in range(6):
            ang = math.pi / 3.0 * k
            px = gx + HEX_R * math.cos(ang)
            py = gy + HEX_R * math.sin(ang)
            pts.append("{0},{1}".format(sx(px), sy(py)))
        parts.append(
            '<polygon points="{0}" fill="{1}" stroke="#000000" stroke-width="0.6"/>'.format(
                " ".join(pts), fill
            )
        )

    def polyline(points, color, dash=None):
        if not points:
            return ""
        pts = []
        for p in points:
            gx, gy = project(as_float3(p), origin, dirv, sidev)
            pts.append("{0},{1}".format(sx(gx), sy(gy)))
        dash_attr = ""
        if dash:
            dash_attr = ' stroke-dasharray="{0}"'.format(dash)
        return (
            '<polyline points="{0}" fill="none" stroke="{1}" '
            'stroke-width="1.5"{2}/>'.format(" ".join(pts), color, dash_attr)
        )

    parts.append(polyline(left_edge, "#999999", "6,4"))
    parts.append(polyline(right_edge, "#999999", "6,4"))
    parts.append(polyline(route, "#000000"))

    # Objects (scanbox) with their ClassName as a label.
    for name, pos in objects:
        gx, gy = project(pos, origin, dirv, sidev)
        x = sx(gx)
        y = sy(gy)
        parts.append(
            '<circle cx="{0}" cy="{1}" r="3" fill="#ff6f00" '
            'stroke="#000000" stroke-width="1"/>'.format(x, y)
        )
        parts.append(
            '<text x="{0}" y="{1}" font-size="10" fill="#b34000">{2}</text>'.format(
                x + 4.0, y - 4.0, name
            )
        )

    # Header: grid status and counts.
    parts.append(
        '<text x="{0}" y="18" font-size="12" fill="#000000">'
        'grid {1} rows={2} width={3} cells={4}</text>'.format(
            margin, status, rows, int(grid.get("Width", 0)), len(cells)
        )
    )

    parts.append('</svg>')

    if not out_path.lower().endswith(".svg"):
        out_path += ".svg"
    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(parts))

    print("wrote {0} ({1} cells, {2} objects)".format(out_path, len(cells), len(objects)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
