//! dmWorldPoiConfig — schema of the generated world_poi.json.
//!
//! The JSON is produced by a separate generator (map/Configs/world_poi.json) and
//! only consumed here; this file holds the typed structs so dmJsonFile<T> can load
//! it. Fields serialize by exact name (no "m_" prefix), matching the JSON keys.

//! Одна локация карты (город/лагерь/холм/...): позиция + тип.
class dmWorldPoiLocation
{
	string Name;      // имя локации
	string Type;      // "Capital"/"City"/"Village"/"Camp"/"Local"/"Hill"/"ViewPoint"/"Marine"/"RailroadStation"/"Ruin"/"LocalOffice"
	vector Position;  // [x, 0, z] (2D-позиция; высота земли резолвится в рантайме)
}

//! Отдельностоящий объект-точка интереса (здание, колодец, ...).
class dmWorldPoiPoint
{
	string Class;     // конфиг-класс здания/объекта
	string Type;      // строка типа POI ("WATER"/"POLICE"/... см. dmWorldPOIType)
	vector Position;  // [x, y, z]
	string Location;  // имя родительской локации ("" = отдельностоящий POI)
}

//! Корень world_poi.json.
class dmWorldPoiConfig : dmJsonConfigBase
{
	string World;                                  // имя карты
	ref array<ref dmWorldPoiLocation> Locations;
	ref array<ref dmWorldPoiPoint> Points;
}
