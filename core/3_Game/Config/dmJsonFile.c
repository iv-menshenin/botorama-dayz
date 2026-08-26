//! dmJsonFile — generic JSON config reader/writer (reusable, mod-agnostic).
//!
//! Thin, dependency-free wrapper over the engine's JsonFileLoader that adds:
//!  - single-file load/save and whole-directory ("*.json") loading;
//!  - optional schema versioning: if T inherits dmJsonConfigBase, Load() runs
//!    FixVersion() when the file's version is older and writes the migrated
//!    config back to the same path.
//!
//! Missing files are NOT auto-created: Load() returns false when the file does
//! not exist; the owner decides what to do (typically Defaults() + Save()).

class dmJsonFile<Class T>
{
	private string m_Path;
	private ref array<string> m_Errors;

	void dmJsonFile(string path)
	{
		m_Path = path;
		m_Errors = new array<string>();
	}

	//! Load the file at m_Path into config. Returns false when the file is
	//! missing or cannot be parsed. Runs version migration (FixVersion +
	//! write-back) when T inherits dmJsonConfigBase and the version is older.
	bool Load(out T config)
	{
		return LoadFrom(m_Path, config);
	}

	//! Serialize config back to the file at m_Path.
	bool Save(T config)
	{
		string error;
		if (!JsonFileLoader<T>.SaveFile(m_Path, config, error))
		{
			m_Errors.Insert(error);
			return false;
		}
		return true;
	}

	//! Load every "*.json" file in the directory (non-recursive) into configs.
	//! Returns true when at least one file was loaded.
	bool LoadDir(out array<ref T> configs)
	{
		if (!configs)
			configs = new array<ref T>();

		if (!FileExist(m_Path))
			return false;

		string fileName;
		FileAttr fileAttr;
		FindFileHandle handle = FindFile(m_Path + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES);

		bool hasMatch = fileName != "";
		while (hasMatch)
		{
			T config;
			if (LoadFrom(m_Path + "/" + fileName, config))
				configs.Insert(config);

			hasMatch = FindNextFile(handle, fileName, fileAttr);
		}
		CloseFindFile(handle);

		return configs.Count() > 0;
	}

	//! Accumulated error messages (newline-separated); empty when none.
	string Errors()
	{
		if (!m_Errors || !m_Errors.Count())
			return "";

		string result;
		foreach (string err : m_Errors)
			result += err + "\n";
		return result;
	}

	//! Convenience: load a single file (errors returned via `error`).
	static bool Load(string path, out T config, out string error)
	{
		dmJsonFile<T> reader = new dmJsonFile<T>(path);
		bool ok = reader.Load(config);
		error = reader.Errors();
		return ok;
	}

	//! Convenience: save a single file (errors returned via `error`).
	static bool Save(string path, T config, out string error)
	{
		dmJsonFile<T> reader = new dmJsonFile<T>(path);
		bool ok = reader.Save(config);
		error = reader.Errors();
		return ok;
	}

	//! Convenience: load a directory of "*.json" files.
	static bool LoadDir(string path, out array<ref T> configs)
	{
		dmJsonFile<T> reader = new dmJsonFile<T>(path);
		return reader.LoadDir(configs);
	}

	//! Create the parent directory chain of a file path (handles the "$profile:"
	//! prefix and relative paths alike). No-op when there is no "/" in the path.
	static void EnsureDirectory(string path)
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

	//! Read + parse + optional version migration for an explicit file path.
	private bool LoadFrom(string path, out T config)
	{
		if (!FileExist(path))
			return false;

		string error;
		if (!JsonFileLoader<T>.LoadFile(path, config, error))
		{
			m_Errors.Insert(error);
			return false;
		}

		dmJsonConfigBase base = dmJsonConfigBase.Cast(config);
		if (base)
		{
			int before = base.GetVersion();
			base.FixVersion();
			if (base.GetVersion() != before)
			{
				//! Migrated — persist the updated config back to the same path.
				string saveError;
				if (!JsonFileLoader<T>.SaveFile(path, config, saveError))
					m_Errors.Insert(saveError);
			}
		}

		return true;
	}
}
