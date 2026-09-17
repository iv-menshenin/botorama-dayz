//! dmRoadProbe — road discovery "walker" (iteration 1).
//!
//! Discrete, synchronous, step-capped. Follows a road centerline from a drivable
//! seed using a double-angle direction estimate, perpendicular edge re-centering
//! and a turn fan. See docs/plans/road-discovery.md for the exact algorithm.

//! Result of one walk: the centerline polyline, per-point width and surface
//! class, plus the terminal status and step count.
class dmRoadProbeResult
{
	ref array<vector> Points;   // centerline points (x, y=SurfaceY, z)
	ref array<float>  Widths;   // road width at each point
	ref array<int>    Surfaces; // surface class at each point (dmRoadSurfaceClass)
	string Status;              // "ok" | "deadend" | "loop" | "maxsteps" | "nodir" | "norseed"
	int    Steps;               // steps performed
}

class dmRoadProbe
{
	static dmRoadProbeResult Walk(vector seed)
	{
		dmRoadProbeResult res = new dmRoadProbeResult();
		res.Points = new array<vector>();
		res.Widths = new array<float>();
		res.Surfaces = new array<int>();
		res.Status = "ok";
		res.Steps = 0;

		float sx = seed[0];
		float sz = seed[2];

		if (!dmRoadSensor.IsDrivable(sx, sz))
		{
			res.Status = "norseed";
			dmBotLog.Error("[ROAD] Walk: seed not drivable (" + sx + "," + sz + ")");
			return res;
		}

		vector p = Vector(sx, 0.0, sz);
		vector dir = FindDirection(p);
		if (dir == vector.Zero)
		{
			res.Status = "nodir";
			dmBotLog.Error("[ROAD] Walk: no road direction at seed (" + sx + "," + sz + ")");
			return res;
		}

		float py = GetGame().SurfaceY(sx, sz);
		res.Points.Insert(Vector(sx, py, sz));
		res.Widths.Insert(0.0);
		res.Surfaces.Insert(dmRoadSensor.Classify(sx, sz));

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
					res.Status = "deadend";
					break;
				}
				fx = p[0] + dir[0] * DM_ROAD_STEP;
				fz = p[2] + dir[2] * DM_ROAD_STEP;
				if (!dmRoadSensor.IsDrivable(fx, fz))
				{
					res.Status = "deadend";
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
			res.Points.Insert(Vector(p[0], py, p[2]));
			res.Widths.Insert(width);
			res.Surfaces.Insert(surf);
			res.Steps = res.Steps + 1;
			step = step + 1;

			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[ROAD] step=" + step + " p=(" + p[0] + "," + p[2] + ")");
			dmBotLog.Debug("[ROAD] w=" + width + " d=(" + dir[0] + "," + dir[2] + ") surf=" + surf);
			#endif

			if (step > DM_ROAD_MIN_CLOSE_STEPS && vector.DistanceSq(p, seed) < DM_ROAD_CLOSE_DIST * DM_ROAD_CLOSE_DIST)
			{
				res.Status = "loop";
				break;
			}
		}

		if (res.Status == "ok" && step >= DM_ROAD_MAX_STEPS)
			res.Status = "maxsteps";

		return res;
	}

	//! Direction of the road at P via the double-angle trick: sample 24 points on a
	//! circle of radius DM_ROAD_DIR_RADIUS, accumulate cos(2θ)/sin(2θ) over drivable
	//! samples, then θ = 0.5·atan2(Σsin2θ, Σcos2θ). vector.Zero when no drivable sample.
	private static vector FindDirection(vector p)
	{
		float sumX = 0.0;
		float sumZ = 0.0;
		int i;
		float angle;
		float px;
		float pz;
		for (i = 0; i < 24; i++)
		{
			angle = i * 15.0 * Math.DEG2RAD;
			px = p[0] + Math.Cos(angle) * DM_ROAD_DIR_RADIUS;
			pz = p[2] + Math.Sin(angle) * DM_ROAD_DIR_RADIUS;
			if (dmRoadSensor.IsDrivable(px, pz))
			{
				sumX = sumX + Math.Cos(2.0 * angle);
				sumZ = sumZ + Math.Sin(2.0 * angle);
			}
		}

		if (Math.AbsFloat(sumX) < 0.0001 && Math.AbsFloat(sumZ) < 0.0001)
			return vector.Zero;

		float theta = 0.5 * Math.Atan2(sumZ, sumX);
		return Vector(Math.Cos(theta), 0.0, Math.Sin(theta));
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
};
