//! dmRoadGraphIO — persist the discovered road graph to JSON.

class dmRoadGraphIO
{
	//! Save the graph to path (creating the parent directory). Returns false
	//! when the file could not be written.
	static bool Save(dmRoadGraph graph, string path)
	{
		EnsureDirectory(path);
		string error;
		if (!JsonFileLoader<dmRoadGraph>.SaveFile(path, graph, error))
		{
			dmBotLog.Error("[ROADG] save failed: " + path + ": " + error);
			return false;
		}
		return true;
	}

	//! Create the parent directory chain of a file path (handles the "$profile:"
	//! prefix and relative paths alike).
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
