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

	//! Длительность анимации рта (сек). Запасное значение 1.0.
	static float GetTalkDuration(int lineId)
	{
		switch (lineId)
		{
		case dmVoiceLine.DM_VOICE_TEST:
			return DM_VOICE_TALK_DURATION_TEST;
		}
		return 1.0;
	}
}
