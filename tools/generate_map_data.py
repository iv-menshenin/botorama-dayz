#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate world_poi.json + buildings_interior.json for a DayZ map (botorama).

Inputs:
  - worldnames dump (e2e `worldnames` op result): profile e2e/out/worldnames.result.json
    -> Dump lines "world=...", "count=N", then N lines "i|key|name|type|x|z".
  - mapgroupproto.xml (mission CE data) -> building classes + interior loot points.

Outputs (written to --out dir, default the profile map/ dir):
  - world_poi.json          : named locations (Name/Type/Position[x,0,z]).
  - buildings_interior.json : building classes (ClassName/Type/Points[[x,y,z],...]).

Classification of building classes follows docs/research/world-poi.md §3 (reverse
engineered from the Chernarus data/map/). Unmatched classes -> GENERIC.
"""
import json
import sys
import xml.etree.ElementTree as ET

# Ordered classification: first matching prefix wins.
CLASSIFICATION = [
    (("Land_Misc_Well_Pump",), "WATER"),
    (("Land_City_PoliceStation", "Land_Village_PoliceStation"), "POLICE"),
    (("Land_City_FireStation", "Land_Mil_FireStation"), "FIRE"),
    (("Land_City_Hospital", "Land_Medical_Tent", "Land_Village_HealthCare"), "MEDICAL"),
    (("Land_FuelStation",), "FUEL"),
    (("Land_Wreck_C130J", "Land_Wreck_V3S", "StaticObj_Wreck", "Wreck_Mi8", "Wreck_UH1Y"),
     "MILITARY_WRECK"),
    (("Land_CementWorks", "Land_CoalPlant", "Land_Factory", "Land_Mine", "Land_Pier_Crane",
      "Land_Power", "Land_Quarry", "Land_Rail_Station", "Land_Rail_Warehouse",
      "Land_Repair_Center", "Land_Sawmill", "Land_Smokestack", "Land_Workshop"),
     "INDUSTRIAL"),
    (("Land_Mil_", "Land_Tisy_", "Land_Airfield_", "Land_Prison_", "Land_Guardhouse",
      "Land_Bunker"),
     "MILITARY"),
    (("Land_House_", "Land_HouseBlock_", "Land_Tenement_", "Land_Camp_House_",
      "Land_Slum_House", "Land_Refugee_tent", "Land_Store_Street"),
     "RESIDENTIAL"),
]


def classify(class_name):
    for prefixes, kind in CLASSIFICATION:
        for p in prefixes:
            if class_name.startswith(p):
                return kind
    return "GENERIC"


def gen_world_poi(result_path, world_name):
    with open(result_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    dump = data["Steps"][0]["Dump"]
    # first two lines: world=... , count=... ; rest are locations
    loc_lines = dump[2:]
    locations = []
    for line in loc_lines:
        parts = line.split("|")
        # i|key|name|type|x|z
        if len(parts) < 6:
            continue
        name = parts[2]
        typ = parts[3]
        try:
            x = float(parts[4])
            z = float(parts[5])
        except ValueError:
            continue
        locations.append({"Name": name, "Type": typ, "Position": [x, 0.0, z]})
    return {"Version": 1, "WorldName": world_name, "Locations": locations}


def gen_buildings(proto_path, world_name):
    tree = ET.parse(proto_path)
    root = tree.getroot()
    buildings = []
    for group in root.findall("group"):
        class_name = group.get("name", "")
        if not class_name:
            continue
        points = []
        for point in group.iter("point"):
            pos = point.get("pos", "")
            if not pos:
                continue
            xyz = pos.split()
            if len(xyz) < 3:
                continue
            try:
                points.append([float(xyz[0]), float(xyz[1]), float(xyz[2])])
            except ValueError:
                continue
        if not points:
            continue
        buildings.append({
            "ClassName": class_name,
            "Type": classify(class_name),
            "Points": points,
        })
    return {"Version": 1, "WorldName": world_name, "Buildings": buildings}


def main():
    world_name = "nasdara"
    result_path = sys.argv[1] if len(sys.argv) > 1 else \
        "/mnt/deep-space/Steam/steamapps/common/DayZServer/profiles-nasdara/dmBotorama/e2e/out/worldnames.result.json"
    proto_path = sys.argv[2] if len(sys.argv) > 2 else \
        "/mnt/deep-space/Steam/steamapps/common/DayZServer/mpmissions/dayzOffline.nasdara/mapgroupproto.xml"
    out_dir = sys.argv[3] if len(sys.argv) > 3 else \
        "/mnt/deep-space/Steam/steamapps/common/DayZServer/profiles-nasdara/dmBotorama/map"

    world_poi = gen_world_poi(result_path, world_name)
    with open(out_dir + "/world_poi.json", "w", encoding="utf-8") as f:
        json.dump(world_poi, f, indent=4, ensure_ascii=False)

    buildings = gen_buildings(proto_path, world_name)
    with open(out_dir + "/buildings_interior.json", "w", encoding="utf-8") as f:
        json.dump(buildings, f, indent=4, ensure_ascii=False)

    # summary
    from collections import Counter
    types = Counter(b["Type"] for b in buildings["Buildings"])
    settlements = [l for l in world_poi["Locations"] if l["Type"] in ("Capital", "City", "Village", "Camp")]
    print("world_poi: %d locations, %d settlements" % (len(world_poi["Locations"]), len(settlements)))
    print("buildings_interior: %d classes" % len(buildings["Buildings"]))
    for t, n in sorted(types.items()):
        print("  %s: %d" % (t, n))


if __name__ == "__main__":
    main()
