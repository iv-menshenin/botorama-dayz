//! dmNameGenerator — генерация имени settlement-бота по классу модели.
//!
//! Ленивый синглтон. Читает names-long.json (schema dmNamesConfig) при первом
//! обращении; при отсутствии/порче файла генерация возвращает фолбэк
//! ("Bot<число>"), не падая.

class dmNameGenerator
{
	static ref dmNameGenerator s_Instance;

	//! Реестр занятых имён (уникальность пути файла персистенции).
	static ref map<string, bool> s_Taken = new map<string, bool>();

	ref dmNamesConfig m_Config;
	bool m_Loaded;

	static dmNameGenerator Get()
	{
		if (!s_Instance)
			s_Instance = new dmNameGenerator();
		return s_Instance;
	}

	//! Загрузить names-long.json один раз. При неудаче m_Config остаётся null.
	void EnsureLoaded()
	{
		if (m_Loaded)
			return;

		m_Loaded = true;

		dmNamesConfig config;
		string error;
		if (!JsonFileLoader<dmNamesConfig>.LoadFile(DM_NAMES_FILE, config, error))
		{
			dmBotLog.Error("", "[NameGen] names load failed: " + DM_NAMES_FILE + ": " + error);
			m_Config = null;
			return;
		}
		m_Config = config;
	}

	//! Сгенерировать имя "Имя Фамилия" по классу модели (dmAI_SurvivorM_Denis и т.п.).
	//! Имя уникально: при коллизии добавляется суффикс " (n)".
	string GenerateName(string modelClass)
	{
		EnsureLoaded();

		string baseName = "Bot" + Math.RandomInt(0, 10000);

		if (m_Config)
		{
			string prefix = "dmAI_Survivor";
			string key = modelClass;
			int idx = modelClass.LastIndexOf(prefix);
			if (idx >= 0)
				key = modelClass.Substring(idx + prefix.Length(), modelClass.Length() - idx - prefix.Length());

			string nationality;
			if (!m_Config.model_mapping.Find(key, nationality))
				nationality = "slavic";

			dmNameNationality nat = m_Config.nationalities.Get(nationality);
			if (!nat)
			{
				baseName = modelClass + "_Bot";
			}
			else
			{
				array<string> firsts;
				if (key.IndexOf("F_") == 0)
					firsts = nat.female_first;
				else
					firsts = nat.male_first;

				if (!firsts || firsts.Count() == 0 || !nat.last || nat.last.Count() == 0)
				{
					baseName = modelClass + "_Bot";
				}
				else
				{
					string firstName = firsts.GetRandomElement();
					string lastName = nat.last.GetRandomElement();
					baseName = firstName + " " + lastName;
				}
			}
		}

		string candidate = baseName;
		int n = 2;
		while (s_Taken.Contains(candidate))
		{
			candidate = baseName + " (" + n + ")";
			n = n + 1;
		}
		s_Taken.Insert(candidate, true);
		return candidate;
	}

	//! Зарегистрировать уже существующее имя (восстановление) — чтобы свежий спавн не коллизировал.
	static void RegisterName(string name)
	{
		s_Taken.Insert(name, true);
	}

	//! Освободить имя (смерть) — имя снова доступно для нового спавна.
	static void ReleaseName(string name)
	{
		s_Taken.Remove(name);
	}
}
