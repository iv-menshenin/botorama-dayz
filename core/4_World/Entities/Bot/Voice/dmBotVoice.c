//! dmBotVoice — маппинг голосовых реплик бота: id -> путь к аудио-файлу и
//! длительность анимации рта. Файлы лежат в $profile:dmBotorama/voices/.
class dmBotVoice
{
	//! Полный путь к .ogg реплики (рантайм, $profile:). Пустая строка = нет реплики.
	static string GetSoundPath(int lineId)
	{
		switch (lineId)
		{
		case dmVoiceLine.DM_VOICE_TEST:
			return DM_VOICE_DIR + "test.ogg";
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
