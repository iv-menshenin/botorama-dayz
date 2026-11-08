//! dmPerfTelemetry — per-frame accumulator for native physics-query telemetry.
//!
//! dmPhysicsProbe folds every wrapped query into a per-frame bucket here (gated
//! with #ifdef DM_BOT_PROFILE at the call site). dmAISurvivor.TickAll calls
//! BeginFrame() once per server frame, folding the bucket into per-frame samples.
//! Dump() writes mean/max/p95 of the per-frame samples to a CSV under
//! $profile:dmBotorama/profile/, mirroring dmBotProfiler's path/timestamp pattern.

class dmPerfTelemetry
{
	//! Current-frame bucket (not yet folded into samples).
	static int s_FrameRayCount;
	static float s_FrameRayLength;
	static int s_FrameScanCount;
	static float s_FrameScanVolume;

	//! Per-frame samples across the whole run (one entry per frame with activity).
	static ref array<float> s_RayCountSamples = new array<float>();
	static ref array<float> s_RayLengthSamples = new array<float>();
	static ref array<float> s_ScanCountSamples = new array<float>();
	static ref array<float> s_ScanVolumeSamples = new array<float>();

	//! Fold one raycast into the current frame bucket (length in meters).
	static void Raycast(float len)
	{
		s_FrameRayCount++;
		s_FrameRayLength = s_FrameRayLength + len;
	}

	//! Fold one scanbox into the current frame bucket (volume in m^3).
	static void ScanBox(float vol)
	{
		s_FrameScanCount++;
		s_FrameScanVolume = s_FrameScanVolume + vol;
	}

	//! Fold the current frame bucket into the per-frame sample arrays and reset.
	//! Called once per server frame by dmAISurvivor.TickAll. Frames without any
	//! query of a kind are NOT recorded (avoids zero-dilution of the stats).
	static void BeginFrame()
	{
		if (s_FrameRayCount > 0)
		{
			s_RayCountSamples.Insert((float)s_FrameRayCount);
			s_RayLengthSamples.Insert(s_FrameRayLength);
		}
		if (s_FrameScanCount > 0)
		{
			s_ScanCountSamples.Insert((float)s_FrameScanCount);
			s_ScanVolumeSamples.Insert(s_FrameScanVolume);
		}
		s_FrameRayCount = 0;
		s_FrameRayLength = 0.0;
		s_FrameScanCount = 0;
		s_FrameScanVolume = 0.0;
	}

	//! Drop all accumulated samples and the current bucket.
	static void Clear()
	{
		s_RayCountSamples.Clear();
		s_RayLengthSamples.Clear();
		s_ScanCountSamples.Clear();
		s_ScanVolumeSamples.Clear();
		s_FrameRayCount = 0;
		s_FrameRayLength = 0.0;
		s_FrameScanCount = 0;
		s_FrameScanVolume = 0.0;
	}

	//! Write the per-frame metric summary to a CSV under $profile:dmBotorama/profile/.
	//! Returns the file path written (empty when no samples were recorded).
	static string Dump()
	{
		if (s_RayCountSamples.Count() == 0 && s_ScanCountSamples.Count() == 0)
			return "";

		string path = "$profile:dmBotorama/profile/telemetry_" + FileTimeStamp() + ".csv";
		EnsureDirectory(path);

		FileHandle file = OpenFile(path, FileMode.WRITE);
		if (!file)
			return "";

		FPrintln(file, "metric,mean,max,p95,samples");
		WriteMetric(file, "raycast_count", s_RayCountSamples);
		WriteMetric(file, "raycast_length_sum", s_RayLengthSamples);
		WriteMetric(file, "scanbox_count", s_ScanCountSamples);
		WriteMetric(file, "scanbox_volume_sum", s_ScanVolumeSamples);

		CloseFile(file);
		return path;
	}

	//! Write one metric row: mean/max/p95 over the per-frame samples.
	private static void WriteMetric(FileHandle file, string name, array<float> samples)
	{
		int n = samples.Count();
		if (n == 0)
		{
			FPrintln(file, name + ",0,0,0,0");
			return;
		}

		float sum = 0.0;
		float max = samples[0];
		int i;
		for (i = 0; i < n; i++)
		{
			float v = samples[i];
			sum = sum + v;
			if (v > max)
				max = v;
		}
		float mean = sum / (float)n;

		array<float> sorted = new array<float>();
		for (i = 0; i < n; i++)
			sorted.Insert(samples[i]);
		sorted.Sort();
		int p95Idx = Math.Floor(0.95 * (float)(n - 1));
		float p95 = sorted[p95Idx];

		FPrintln(file, name + "," + mean + "," + max + "," + p95 + "," + n);
	}

	//! Wall-clock "HH-MM-SS" timestamp (filename-safe).
	private static string FileTimeStamp()
	{
		int hour;
		int minute;
		int second;
		GetHourMinuteSecond(hour, minute, second);
		return hour.ToStringLen(2) + "-" + minute.ToStringLen(2) + "-" + second.ToStringLen(2);
	}

	//! Create the parent directory chain of a file path (mirrors dmBotProfiler).
	private static void EnsureDirectory(string path)
	{
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash < 0)
			return;

		TStringArray comps = new TStringArray();
		path.Substring(0, lastSlash).Split("/", comps);

		int startFrom = 0;
		string dir = "";
		if (comps.Count() > 0 && comps[0] == "$profile:")
		{
			dir = "$profile:";
			startFrom = 1;
		}

		for (int i = startFrom; i < comps.Count(); i++)
		{
			if (dir != "")
				dir += "/";
			dir += comps[i];
			if (!FileExist(dir))
				MakeDirectory(dir);
		}
	}
}
