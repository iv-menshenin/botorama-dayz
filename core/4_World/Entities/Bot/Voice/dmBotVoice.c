//! dmBotVoice — маппинг голосовых реплик бота: id -> имя CfgSoundSets-звукового
//! сета (см. config.cpp) и длительность анимации рта.
class dmBotVoice
{
	//! Имя CfgSoundSets-сета реплики. Пустая строка = нет реплики.
	static string GetSoundSetName(int lineId)
	{
		switch (lineId)
		{
		case dmVoiceLine.DM_VOICE_TEST:
			return "dmBotVoice_test_SoundSet";
		}
		return "";
	}

	//! Длительность анимации рта (сек) — читается из конфига
	//! (CfgSoundSets <set> duration, задано рядом со звуком в config.cpp).
	//! Fallback 1.0, если поле не задано.
	static float GetTalkDuration(int lineId)
	{
		string name = GetSoundSetName(lineId);
		if (name == "")
			return 1.0;

		string path = "CfgSoundSets " + name + " duration";
		if (GetGame().ConfigIsExisting(path))
			return GetGame().ConfigGetFloat(path);

		return 1.0;
	}
}
