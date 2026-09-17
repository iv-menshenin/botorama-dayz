//! dmRoadProbe — road discovery "walker" (iteration 2).
//!
//! Discrete, synchronous, step-capped. From a drivable seed it detects ALL road
//! directions (branches) via a 36-sample density ring, then walks each branch
//! along its centerline using perpendicular edge re-centering and a turn fan.
//! See docs/plans/road-discovery.md for the exact algorithm.

//! One branch: a single road direction followed from the seed.
class dmRoadBranch
{
	ref array<vector> Points;   // centerline points (x, y=SurfaceY, z)
	ref array<float>  Widths;   // road width at each point
	ref array<int>    Surfaces; // surface class at each point (dmRoadSurfaceClass)
	string Status;              // "ok" | "deadend" | "loop" | "maxsteps"
	int    Steps;               // steps performed
}

//! Result of one walk: a branch per found direction plus the overall status.
class dmRoadProbeResult
{
	ref array<ref dmRoadBranch> Branches;  // one branch per found direction
	string Status;              // "ok" | "norseed" | "nodir"
	int    TotalSteps;          // sum of branch steps
}

class dmRoadProbe
{
	static dmRoadProbeResult Walk(vector seed)
	{
		dmRoadProbeResult res = new dmRoadProbeResult();
		res.Branches = new array<ref dmRoadBranch>();
		res.Status = "ok";
		res.TotalSteps = 0;

		float sx = seed[0];
		float sz = seed[2];

		if (!dmRoadSensor.IsDrivable(sx, sz))
		{
			res.Status = "norseed";
			dmBotLog.Error("[ROAD] Walk: seed not drivable (" + sx + "," + sz + ")");
			return res;
		}

		vector p = Vector(sx, 0.0, sz);
		array<vector> dirs = FindBranches(p);
		if (dirs.Count() == 0)
		{
			res.Status = "nodir";
			dmBotLog.Error("[ROAD] Walk: no road directions at seed (" + sx + "," + sz + ")");
			return res;
		}

		int i;
		for (i = 0; i < dirs.Count(); i++)
		{
			dmRoadBranch branch = WalkDir(p, dirs[i], i);
			res.Branches.Insert(branch);
			res.TotalSteps = res.TotalSteps + branch.Steps;
		}

		return res;
	}

	//! All road directions at P: 36 samples on a circle of radius
	//! DM_ROAD_BRANCH_RADIUS (10° each), a wrapped ±3-sample drivable density,
	//! then peaks (density >= DM_ROAD_BRANCH_MIN_DENSITY and >= both neighbours)
	//! with non-max suppression (peaks >= DM_ROAD_BRANCH_MIN_GAP samples apart).
	//! Returns unit direction vectors for the found peak angles.
	static array<vector> FindBranches(vector p)
	{
		array<vector> result = new array<vector>();
		array<bool> drivable = new array<bool>();
		array<int> density = new array<int>();
		array<int> peaks = new array<int>();

		int n = 36;
		int i;
		int j;
		int k;
		int im;
		int ip;
		int d;
		int diff;
		bool farEnough;
		float angle;
		float px;
		float pz;

		for (i = 0; i < n; i++)
		{
			angle = i * 10.0 * Math.DEG2RAD;
			px = p[0] + Math.Cos(angle) * DM_ROAD_BRANCH_RADIUS;
			pz = p[2] + Math.Sin(angle) * DM_ROAD_BRANCH_RADIUS;
			drivable.Insert(dmRoadSensor.IsDrivable(px, pz));
		}

		for (i = 0; i < n; i++)
		{
			d = 0;
			for (j = -3; j <= 3; j++)
			{
				k = i + j;
				if (k < 0)
					k = k + n;
				if (k >= n)
					k = k - n;
				if (drivable[k])
					d = d + 1;
			}
			density.Insert(d);
		}

		for (i = 0; i < n; i++)
		{
			if (density[i] < DM_ROAD_BRANCH_MIN_DENSITY)
				continue;

			im = i - 1;
			if (im < 0)
				im = im + n;
			ip = i + 1;
			if (ip >= n)
				ip = ip - n;
			if (density[i] < density[im] || density[i] < density[ip])
				continue;

			farEnough = true;
			for (j = 0; j < peaks.Count(); j++)
			{
				diff = i - peaks[j];
				if (diff < 0)
					diff = -diff;
				if (diff > n - diff)
					diff = n - diff;
				if (diff < DM_ROAD_BRANCH_MIN_GAP)
				{
					farEnough = false;
					break;
				}
			}
			if (!farEnough)
				continue;

			peaks.Insert(i);
			angle = i * 10.0 * Math.DEG2RAD;
			result.Insert(Vector(Math.Cos(angle), 0.0, Math.Sin(angle)));
		}

		return result;
	}

	//! Walk one branch from the seed along the given direction. Re-centers onto
	//! the centerline each step, turns at corners via the fan, and stops on a
	//! dead end, loop closure or the step cap. branchIndex is only used for the
	//! per-step debug telemetry.
	private static dmRoadBranch WalkDir(vector seed, vector dir, int branchIndex)
	{
		dmRoadBranch branch = new dmRoadBranch();
		branch.Points = new array<vector>();
		branch.Widths = new array<float>();
		branch.Surfaces = new array<int>();
		branch.Status = "ok";
		branch.Steps = 0;

		vector p = seed;

		float py = GetGame().SurfaceY(p[0], p[2]);
		branch.Points.Insert(Vector(p[0], py, p[2]));
		branch.Widths.Insert(0.0);
		branch.Surfaces.Insert(dmRoadSensor.Classify(p[0], p[2]));

		int step = 0;
		float fx;
		float fz;
		vector next;
		float width;
		vector center;
		vector toCenter;
		int surf;
		while (step < DM_ROAD_MAX_STEPS)
		{
			fx = p[0] + dir[0] * DM_ROAD_STEP;
			fz = p[2] + dir[2] * DM_ROAD_STEP;
			if (!dmRoadSensor.IsDrivable(fx, fz))
			{
				dir = TurnFan(p, dir);
				if (dir == vector.Zero)
				{
					branch.Status = "deadend";
					break;
				}
				fx = p[0] + dir[0] * DM_ROAD_STEP;
				fz = p[2] + dir[2] * DM_ROAD_STEP;
				if (!dmRoadSensor.IsDrivable(fx, fz))
				{
					branch.Status = "deadend";
					break;
				}
			}

			next = Vector(fx, 0.0, fz);
			width = 0.0;
			center = ReCenter(next, dir, width);
			toCenter = center - p;
			if (toCenter.LengthSq() > 0.0001)
			{
				toCenter.Normalize();
				dir = toCenter;
			}
			p = center;

			py = GetGame().SurfaceY(p[0], p[2]);
			surf = dmRoadSensor.Classify(p[0], p[2]);
			branch.Points.Insert(Vector(p[0], py, p[2]));
			branch.Widths.Insert(width);
			branch.Surfaces.Insert(surf);
			branch.Steps = branch.Steps + 1;
			step = step + 1;

			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[ROAD] b=" + branchIndex + " step=" + step + " p=(" + p[0] + "," + p[2] + ")");
			dmBotLog.Debug("[ROAD] w=" + width + " d=(" + dir[0] + "," + dir[2] + ") surf=" + surf);
			#endif

			if (step > DM_ROAD_MIN_CLOSE_STEPS && vector.DistanceSq(p, seed) < DM_ROAD_CLOSE_DIST * DM_ROAD_CLOSE_DIST)
			{
				branch.Status = "loop";
				break;
			}
		}

		if (branch.Status == "ok" && step >= DM_ROAD_MAX_STEPS)
			branch.Status = "maxsteps";

		return branch;
	}

	//! Re-center P onto the road centerline by scanning the perpendicular N from P in
	//! both directions. left/right = drivable distance to each edge; width = left+right.
	private static vector ReCenter(vector p, vector dir, out float width)
	{
		float nx = -dir[2];
		float nz = dir[0];

		float left = 0.0;
		float right = 0.0;
		float dist = DM_ROAD_EDGE_STEP;
		float px;
		float pz;

		while (dist <= DM_ROAD_MAX_HALF_WIDTH)
		{
			px = p[0] + nx * dist;
			pz = p[2] + nz * dist;
			if (!dmRoadSensor.IsDrivable(px, pz))
				break;
			left = dist;
			dist = dist + DM_ROAD_EDGE_STEP;
		}

		dist = DM_ROAD_EDGE_STEP;
		while (dist <= DM_ROAD_MAX_HALF_WIDTH)
		{
			px = p[0] - nx * dist;
			pz = p[2] - nz * dist;
			if (!dmRoadSensor.IsDrivable(px, pz))
				break;
			right = dist;
			dist = dist + DM_ROAD_EDGE_STEP;
		}

		width = left + right;
		float shift = (left - right) * 0.5;
		return Vector(p[0] + nx * shift, 0.0, p[2] + nz * shift);
	}

	//! Turn fan: try turning D by ±{15,30,45,60,75}°; return the first direction whose
	//! STEP-ahead point is drivable, else vector.Zero (dead end).
	private static vector TurnFan(vector p, vector dir)
	{
		array<float> angles = {15.0, 30.0, 45.0, 60.0, 75.0};
		int i;
		float angle;
		vector d;
		float px;
		float pz;
		for (i = 0; i < angles.Count(); i++)
		{
			angle = angles[i] * Math.DEG2RAD;
			d = Rotate2D(dir, angle);
			px = p[0] + d[0] * DM_ROAD_STEP;
			pz = p[2] + d[2] * DM_ROAD_STEP;
			if (dmRoadSensor.IsDrivable(px, pz))
				return d;

			d = Rotate2D(dir, -angle);
			px = p[0] + d[0] * DM_ROAD_STEP;
			pz = p[2] + d[2] * DM_ROAD_STEP;
			if (dmRoadSensor.IsDrivable(px, pz))
				return d;
		}
		return vector.Zero;
	}

	//! Rotate a 2D vector (x,z) by angle (radians); Y stays 0.
	private static vector Rotate2D(vector v, float angle)
	{
		float ca = Math.Cos(angle);
		float sa = Math.Sin(angle);
		return Vector(v[0] * ca - v[2] * sa, 0.0, v[0] * sa + v[2] * ca);
	}

	//! Walk one branch for graph building. Like WalkDir but with junction
	//! detection (every DM_ROAD_JUNCTION_CHECK_STEP steps, a road direction that
	//! is neither the current direction nor the reverse of cameFrom stops the
	//! walk with Status="junction") and visited-cell dedup (a point whose cell
	//! is already in `visited` stops with Status="visited"). Otherwise the
	//! statuses match WalkDir (ok/deadend/loop/maxsteps). cameFrom is the
	//! direction travelled to reach the seed (vector.Zero for a fresh seed).
	static dmRoadBranch WalkDirBranch(vector seed, vector dir, vector cameFrom, map<string,bool> visited)
	{
		dmRoadBranch branch = new dmRoadBranch();
		branch.Points = new array<vector>();
		branch.Widths = new array<float>();
		branch.Surfaces = new array<int>();
		branch.Status = "ok";
		branch.Steps = 0;

		vector p = seed;

		float py = GetGame().SurfaceY(p[0], p[2]);
		branch.Points.Insert(Vector(p[0], py, p[2]));
		branch.Widths.Insert(0.0);
		branch.Surfaces.Insert(dmRoadSensor.Classify(p[0], p[2]));

		int step = 0;
		float fx;
		float fz;
		vector next;
		float width;
		vector center;
		vector toCenter;
		int surf;
		string key;
		while (step < DM_ROAD_MAX_STEPS)
		{
			fx = p[0] + dir[0] * DM_ROAD_STEP;
			fz = p[2] + dir[2] * DM_ROAD_STEP;
			if (!dmRoadSensor.IsDrivable(fx, fz))
			{
				dir = TurnFan(p, dir);
				if (dir == vector.Zero)
				{
					branch.Status = "deadend";
					break;
				}
				fx = p[0] + dir[0] * DM_ROAD_STEP;
				fz = p[2] + dir[2] * DM_ROAD_STEP;
				if (!dmRoadSensor.IsDrivable(fx, fz))
				{
					branch.Status = "deadend";
					break;
				}
			}

			next = Vector(fx, 0.0, fz);
			width = 0.0;
			center = ReCenter(next, dir, width);
			toCenter = center - p;
			if (toCenter.LengthSq() > 0.0001)
			{
				toCenter.Normalize();
				dir = toCenter;
			}
			p = center;

			if (step > DM_ROAD_MIN_CLOSE_STEPS && vector.DistanceSq(p, seed) < DM_ROAD_CLOSE_DIST * DM_ROAD_CLOSE_DIST)
			{
				branch.Status = "loop";
				break;
			}

			key = CellKey(p[0], p[2], DM_ROAD_DEDUP_CELL);
			if (visited.Contains(key))
			{
				branch.Status = "visited";
				break;
			}

			py = GetGame().SurfaceY(p[0], p[2]);
			surf = dmRoadSensor.Classify(p[0], p[2]);
			branch.Points.Insert(Vector(p[0], py, p[2]));
			branch.Widths.Insert(width);
			branch.Surfaces.Insert(surf);
			branch.Steps = branch.Steps + 1;
			step = step + 1;

			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[ROAD] branch step=" + step + " p=(" + p[0] + "," + p[2] + ") surf=" + surf);
			#endif

			if (step % DM_ROAD_JUNCTION_CHECK_STEP == 0 && IsJunction(p, dir, cameFrom))
			{
				branch.Status = "junction";
				break;
			}
		}

		if (branch.Status == "ok" && step >= DM_ROAD_MAX_STEPS)
			branch.Status = "maxsteps";

		return branch;
	}

	//! True when P has a road direction that is neither the current travel
	//! direction dir nor the reverse of cameFrom — i.e., the road splits here.
	private static bool IsJunction(vector p, vector dir, vector cameFrom)
	{
		vector back = Vector(-dir[0], 0.0, -dir[2]);
		if (cameFrom != vector.Zero)
			back = Vector(-cameFrom[0], 0.0, -cameFrom[2]);

		array<vector> dirs = FindBranches(p);
		int i;
		for (i = 0; i < dirs.Count(); i++)
		{
			if (AngleDeg(dirs[i], dir) < DM_ROAD_BRANCH_MATCH_ANGLE)
				continue;
			if (AngleDeg(dirs[i], back) < DM_ROAD_BRANCH_MATCH_ANGLE)
				continue;
			return true;
		}
		return false;
	}

	//! Unsigned angle (degrees) between two unit vectors in the XZ plane.
	static float AngleDeg(vector a, vector b)
	{
		float cross = a[0] * b[2] - a[2] * b[0];
		float dot = a[0] * b[0] + a[2] * b[2];
		float angleRad = Math.Atan2(Math.AbsFloat(cross), dot);
		return angleRad * Math.RAD2DEG;
	}

	//! Dedup-grid cell key "gx:gz" for a point, using the given cell size.
	static string CellKey(float x, float z, float cellSize)
	{
		int gx = Math.Floor(x / cellSize);
		int gz = Math.Floor(z / cellSize);
		return gx.ToString() + ":" + gz.ToString();
	}
};
