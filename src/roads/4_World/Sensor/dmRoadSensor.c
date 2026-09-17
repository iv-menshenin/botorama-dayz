//! dmRoadSensor — surface material classification for road discovery.
//!
//! Single source of truth is GetGame().GetSurface(Roadway, CLOSEST): on a road it
//! returns the road proxy surface (asphalt_ext / concrete_ext / dirt_ext), whose
//! friction (CfgSurfaces friction) drives the drivable/non-drivable split:
//! >= DM_ROAD_FRICTION_MIN is a road (paved if >= DM_ROAD_FRICTION_PAVED, otherwise
//! dirt). Off-road it returns terrain (cp_dirt / cp_grass, null object). Non-drivable
//! surfaces keep a name-based material taxonomy (see docs/plans/road-discovery.md).

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
	private static ref map<string,float> s_Friction;

	//! Classify the surface material at (x,z).
	static int Classify(float x, float z)
	{
		float terrainY = GetGame().SurfaceY(x, z);
		SurfaceDetectionParameters p = new SurfaceDetectionParameters();
		p.type = SurfaceDetectionType.Roadway;
		p.position = Vector(x, terrainY, z);
		p.rsd = RoadSurfaceDetection.CLOSEST;
		SurfaceDetectionResult res = new SurfaceDetectionResult();
		GetGame().GetSurface(p, res);

		string type = "";
		if (res.surface)
			type = res.surface.GetSurfaceType();
		type.ToLower();
		float f = GetFriction(type);
		if (f >= DM_ROAD_FRICTION_MIN)
		{
			if (f >= DM_ROAD_FRICTION_PAVED)
				return dmRoadSurfaceClass.ROAD_PAVED;
			return dmRoadSurfaceClass.ROAD_DIRT;
		}
		// Not a road: keep the material taxonomy by name.
		if (type.Contains("roof") || type.Contains("planks") || type.Contains("tiles"))
			return dmRoadSurfaceClass.ROAD_STRUCTURE;
		if (type.Contains("water") || type.Contains("pond") || type.Contains("sea"))
			return dmRoadSurfaceClass.ROAD_WATER;
		if (type.Contains("broadleaf") || type.Contains("conifer"))
			return dmRoadSurfaceClass.ROAD_FOREST;
		if (type.Contains("grass"))
			return dmRoadSurfaceClass.ROAD_GRASS;
		// Bare earth (dirt) and anything else: not a road.
		return dmRoadSurfaceClass.ROAD_UNKNOWN;
	}

	//! Friction of a surface type (CfgSurfaces), cached per type.
	private static float GetFriction(string type)
	{
		if (!s_Friction)
			s_Friction = new map<string,float>();
		float f;
		if (s_Friction.Find(type, f))
			return f;
		f = GetGame().ConfigGetFloat("CfgSurfaces " + type + " friction");
		s_Friction.Insert(type, f);
		return f;
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
			if (name == "" || value == "")
				continue;
			name.ToLower();
			value.ToLower();
			if (name == "class" && value == "road")
				return true;
		}
		return false;
	}
};
