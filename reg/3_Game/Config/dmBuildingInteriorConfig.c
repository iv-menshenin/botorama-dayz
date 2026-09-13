//! dmBuildingInteriorConfig — schema of the generated buildings_interior.json.
//!
//! The JSON is produced by a separate generator (map/Configs/buildings_interior.json)
//! and only consumed here; this file holds the typed structs so dmJsonFile<T> can
//! load it. Fields serialize by exact name (no "m_" prefix), matching the JSON keys.

//! Один класс здания: относительные точки лута внутри него.
class dmBuildingInteriorEntry
{
	string Class;              // конфиг-класс здания (GetType())
	string Type;               // строка типа POI ("WATER"/"POLICE"/... → dmWorldPOIType)
	ref array<vector> Points;  // относительные координаты [x,y,z] в локальном пространстве здания
}

//! Корень buildings_interior.json.
class dmBuildingInteriorConfig : dmJsonConfigBase
{
	string World;                                  // имя карты ("chernarusplus")
	ref array<ref dmBuildingInteriorEntry> Buildings;
}
