#!/usr/bin/env python3
"""render_honeycomb.py — render a dmRoadHoneycomb grid dump to an SVG.

Draws everything in the GRID coordinate system (rows -> +X axis, columns ->
+Y axis). World points (route, road edges, objects) are projected into grid
space through the territory frame (origin/dir/side) from the dump:

    along  = (p - origin) . dir
    across = (p - origin) . side
    px     = along / cellSize        (cellSize = 0.66 m)
    py     = across / cellSize

Input 1 — griddump.json (produced by the E2E `griddump` op):

    {
      "Origin":    [x, y, z],
      "Dir":       [dx, dy, dz],
      "Side":      [sx, sy, sz],
      "Rows":      393,
      "Width":     91,
      "HalfWidth": 45,
      "Status":    "done",
      "CellCount": 35763,
      "Cells":     [ { "Row": 0, "Col": -45, "Color": 2, "X": .., "Z": .. }, ... ],
      "Route":     [ [x, y, z], ... ],
      "LeftEdge":  [ [x, y, z], ... ],
      "RightEdge": [ [x, y, z], ... ]
    }

    Cell "Color": 0 = unset (white), 1 = RED, 2 = GREEN, 3 = SAFE (dark green).

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
import os
import sys

CELL_SIZE = 0.66

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
    return along / CELL_SIZE, across / CELL_SIZE


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
    half_width = int(grid["HalfWidth"])
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

    # Territory bounds in grid units, padded half a cell so cell rects fit.
    min_x = -0.5
    max_x = float(rows) - 0.5
    min_y = -float(half_width) - 0.5
    max_y = float(half_width) + 0.5

    scale = 5.0
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

    # Cells (one unit rect centered on its grid coordinate).
    for cell in cells:
        row = int(cell["Row"])
        col = int(cell["Col"])
        color = int(cell["Color"])
        fill = COLOR_FILL.get(color, "#ffffff")
        x = sx(float(row) - 0.5)
        y = sy(float(col) + 0.5)
        parts.append(
            '<rect x="{0}" y="{1}" width="{2}" height="{2}" fill="{3}"/>'.format(
                x, y, scale, fill
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
