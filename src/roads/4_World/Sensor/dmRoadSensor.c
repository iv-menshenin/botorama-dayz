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

	//! Full surface info at (x,z): material name, friction, and class. One GetSurface.
	static void SampleSurface(float x, float z, out string name, out float friction, out int cls)
	{
		name = "";
		friction = 0.0;
		cls = dmRoadSurfaceClass.ROAD_UNKNOWN;
		float terrainY = GetGame().SurfaceY(x, z);
		SurfaceDetectionParameters p = new SurfaceDetectionParameters();
		p.type = SurfaceDetectionType.Roadway;
		p.position = Vector(x, terrainY, z);
		p.rsd = RoadSurfaceDetection.CLOSEST;
		SurfaceDetectionResult res = new SurfaceDetectionResult();
		GetGame().GetSurface(p, res);
		if (res.surface)
			name = res.surface.GetSurfaceType();
		name.ToLower();
		friction = GetFriction(name);
		if (friction >= DM_ROAD_FRICTION_MIN)
		{
			if (friction >= DM_ROAD_FRICTION_PAVED)
				cls = dmRoadSurfaceClass.ROAD_PAVED;
			else
				cls = dmRoadSurfaceClass.ROAD_DIRT;
		}
		else
		{
			if (name.Contains("roof") || name.Contains("planks") || name.Contains("tiles"))
				cls = dmRoadSurfaceClass.ROAD_STRUCTURE;
			else if (name.Contains("water") || name.Contains("pond") || name.Contains("sea"))
				cls = dmRoadSurfaceClass.ROAD_WATER;
			else if (name.Contains("broadleaf") || name.Contains("conifer"))
				cls = dmRoadSurfaceClass.ROAD_FOREST;
			else if (name.Contains("grass"))
				cls = dmRoadSurfaceClass.ROAD_GRASS;
			else
				cls = dmRoadSurfaceClass.ROAD_UNKNOWN;
		}
	}

	//! Classify the surface material at (x,z) via SampleSurface.
	static int Classify(float x, float z)
	{
		string name;
		float friction;
		int cls;
		SampleSurface(x, z, name, friction, cls);
		return cls;
	}

	//! Mnemonic surface category.
	static string Category(string name)
	{
		name.ToLower();
		if (name.Contains("asphalt") || name.Contains("concrete") || name.Contains("stone"))
			return "paved";
		if (name.Contains("dirt"))
			return "dirt";
		if (name.Contains("gravel"))
			return "gravel";
		return "unknown";
	}

	//! Category at a point (SampleSurface -> Category).
	static string CategoryAt(float x, float z)
	{
		string name;
		float friction;
		int cls;
		SampleSurface(x, z, name, friction, cls);
		return Category(name);
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

	//! True if the object is a road. Road proxies carry LB/PB (one segment end) and
	//! LE/PE (the other). GetLODByName crashes on some road proxies (streaming race),
	//! so detect via memory points instead of the "geometry" LOD.
	static bool IsRoadObject(Object obj)
	{
		if (!obj)
			return false;
		if (obj.MemoryPointExists("LB") && obj.MemoryPointExists("PB"))
			return true;
		if (obj.MemoryPointExists("LE") && obj.MemoryPointExists("PE"))
			return true;
		return false;
	}
};
