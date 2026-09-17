//! dmRoadSensor — surface material classification for road discovery.
//!
//! Single source of truth is GetGame().SurfaceGetType(x, z, out string type):
//! the engine fills the surface material name, which we classify by substring
//! (order matters — see docs/plans/road-discovery.md).

enum dmRoadSurfaceClass
{
	ROAD_UNKNOWN = 0,
	ROAD_PAVED,      // asphalt / concrete
	ROAD_DIRT,       // dirt
	ROAD_GRASS,      // grass
	ROAD_FOREST,     // forest floor
	ROAD_STRUCTURE,  // roof / decking
	ROAD_WATER       // water
};

class dmRoadSensor
{
	//! Classify the surface material at (x,z).
	static int Classify(float x, float z)
	{
		string type = "";
		GetGame().SurfaceGetType(x, z, type);
		type.ToLower();
		if (type.Contains("roof") || type.Contains("planks") || type.Contains("tiles"))
			return dmRoadSurfaceClass.ROAD_STRUCTURE;
		if (type.Contains("water") || type.Contains("pond") || type.Contains("sea"))
			return dmRoadSurfaceClass.ROAD_WATER;
		if (type.Contains("concrete"))
			return dmRoadSurfaceClass.ROAD_PAVED;
		if (type.Contains("dirt"))
			return dmRoadSurfaceClass.ROAD_DIRT;
		if (type.Contains("broadleaf") || type.Contains("conifer"))
			return dmRoadSurfaceClass.ROAD_FOREST;
		if (type.Contains("grass"))
			return dmRoadSurfaceClass.ROAD_GRASS;
		return dmRoadSurfaceClass.ROAD_UNKNOWN;
	}

	//! Is a surface class drivable (a road / dirt track)?
	static bool IsDrivable(int cls)
	{
		return cls == dmRoadSurfaceClass.ROAD_PAVED || cls == dmRoadSurfaceClass.ROAD_DIRT;
	}

	//! Is the point (x,z) drivable?
	static bool IsDrivable(float x, float z)
	{
		return IsDrivable(Classify(x, z));
	}

	//! True if the object is a road (property class=="road" in the "geometry" LOD).
	static bool IsRoadObject(Object obj)
	{
		if (!obj)
			return false;
		LOD geometry = obj.GetLODByName("geometry");
		if (!geometry)
			return false;
		int i;
		for (i = 0; i < geometry.GetPropertyCount(); i++)
		{
			string name = geometry.GetPropertyName(i);
			string value = geometry.GetPropertyValue(i);
			name.ToLower();
			value.ToLower();
			if (name == "class" && value == "road")
				return true;
		}
		return false;
	}
};
