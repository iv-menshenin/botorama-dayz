//! dmBotProfiler — lightweight accumulating profiler.
//!
//! Spans are scope-guards: `dmBotSpan _span = dmBotProfiler.Start("Name");` — the
//! destructor records the elapsed wall time into a global accumulator when the
//! span goes out of scope (any return path). Nothing is logged per-span; the
//! accumulator is dumped to a CSV on demand via the "/prof dump" chat command.
//!
//! Every Start() call site is gated with #ifdef DM_BOT_PROFILE, so a normal build
//! has zero overhead (see cons/4_World/defines.c). Dump/Clear remain callable
//! regardless; with profiling off the map is empty and they are no-ops.

class dmBotSpan
{
	string m_Name;
	float m_Start;

	void dmBotSpan(string name)
	{
		m_Name = name;
		m_Start = GetGame().GetTickTime();
	}

	void ~dmBotSpan()
	{
		dmBotProfiler.Record(m_Name, GetGame().GetTickTime() - m_Start);
	}
}

//! Accumulator for one span name: call count + total/min/max elapsed seconds.
class dmProfEntry
{
	int m_Calls;
	float m_Total;
	float m_Min;
	float m_Max;
}

class dmBotProfiler
{
	static ref map<string, ref dmProfEntry> s_Data = new map<string, ref dmProfEntry>();

	//! Accumulation switch ("/prof start|stop"). Enabled by default so early
	//! initialization is not missed.
	static bool s_Enabled = true;

	//! Open a span. Returns a scope-guard whose destructor records the elapsed
	//! time, or null (no-op) while accumulation is disabled. Call sites are gated
	//! with #ifdef DM_BOT_PROFILE.
	static dmBotSpan Start(string name)
	{
		if (!s_Enabled)
			return null;
		return new dmBotSpan(name);
	}

	//! Enable/disable accumulation ("/prof start|stop"). Enabled by default.
	static void SetEnabled(bool enabled)
	{
		s_Enabled = enabled;
	}

	static bool IsEnabled()
	{
		return s_Enabled;
	}

	//! Fold one observation into the accumulator (called by ~dmBotSpan).
	static void Record(string name, float elapsed)
	{
		if (!s_Data)
			s_Data = new map<string, ref dmProfEntry>();

		dmProfEntry entry;
		if (s_Data.Find(name, entry))
		{
			entry.m_Calls++;
			entry.m_Total += elapsed;
			if (elapsed < entry.m_Min)
				entry.m_Min = elapsed;
			if (elapsed > entry.m_Max)
				entry.m_Max = elapsed;
		}
		else
		{
			entry = new dmProfEntry();
			entry.m_Calls = 1;
			entry.m_Total = elapsed;
			entry.m_Min = elapsed;
			entry.m_Max = elapsed;
			s_Data.Set(name, entry);
		}
	}

	//! Drop all accumulated observations.
	static void Clear()
	{
		if (s_Data)
			s_Data.Clear();
	}

	//! Write the accumulated summary to a CSV file under $profile:dmBotorama/profile/.
	//! Returns the file path written (empty on failure).
	static string Dump()
	{
		if (!s_Data || s_Data.Count() == 0)
			return "";

		string path = "$profile:dmBotorama/profile/profile_" + FileTimeStamp() + ".csv";
		EnsureDirectory(path);

		FileHandle file = OpenFile(path, FileMode.WRITE);
		if (!file)
			return "";

		FPrintln(file, "name,calls,total_ms,avg_ms,min_ms,max_ms");

		TStringArray names = s_Data.GetKeyArray();
		names.Sort();

		foreach (string name : names)
		{
			dmProfEntry entry = s_Data.Get(name);
			if (!entry)
				continue;

			float totalMs = entry.m_Total * 1000.0;
			float avgMs = totalMs / entry.m_Calls;
			float minMs = entry.m_Min * 1000.0;
			float maxMs = entry.m_Max * 1000.0;
			FPrintln(file, name + "," + entry.m_Calls + "," + totalMs + "," + avgMs + "," + minMs + "," + maxMs);
		}

		CloseFile(file);
		return path;
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

	//! Create the parent directory chain of a file path (handles the "$profile:"
	//! prefix and relative paths alike). Mirrors dmJsonFile.EnsureDirectory.
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
