//! dmBotVoiceCenter — центр воспроизведения реплик: решает, КОГДА бот заговорит,
//! держит покатегорийный кулдаун молчания и выбирает случайную реплику из категории.
class dmBotVoiceCenter
{
	private ref dmAISurvivor m_Bot;
	private ref map<dmVoiceCategory, float> m_LastSpokeAt;

	void dmBotVoiceCenter(dmAISurvivor bot)
	{
		m_Bot = bot;
		m_LastSpokeAt = new map<dmVoiceCategory, float>();
	}

	//! Случайная реплика из диапазона [indexFrom..indexTo] категории, С кулдауном категории.
	//! indexTo = -1 → до конца (GetCategoryCount). Возвращает lineId (>=1) или -1 (кулдаун/пусто).
	int Say(dmVoiceCategory cat, int indexFrom = 1, int indexTo = -1)
	{
		return SayImpl(cat, indexFrom, indexTo, true);
	}

	//! То же, но БЕЗ кулдауна (редкие события). Всё равно обновляет m_LastSpokeAt[cat].
	int SayNoCooldown(dmVoiceCategory cat, int indexFrom = 1, int indexTo = -1)
	{
		return SayImpl(cat, indexFrom, indexTo, false);
	}

	private int SayImpl(dmVoiceCategory cat, int indexFrom, int indexTo, bool respectCooldown)
	{
		int count = dmBotVoice.GetCategoryCount(cat);
		if (count <= 0)
			return -1;

		// нормализация диапазона [from..to] внутри [1..count]
		int from = indexFrom;
		int to = indexTo;
		if (from < 1)
			from = 1;
		if (to < 1 || to > count)
			to = count;
		if (from > to)
			from = to;

		float now = GetGame().GetTickTime();

		if (respectCooldown)
		{
			float last;
			if (m_LastSpokeAt.Find(cat, last) && now - last < dmBotVoice.GetCooldown(cat))
				return -1;
		}

		int index = Math.RandomIntInclusive(from, to);
		int lineId = cat * 100 + index;

		m_LastSpokeAt.Set(cat, now);

		dmBotIntent_Voice intent = new dmBotIntent_Voice();
		intent.m_LineId = lineId;
		m_Bot.AddPersonalityIntent(intent);

		#ifdef DM_BOT_DEBUG_VOICE
		string cd = "off";
		if (respectCooldown)
			cd = "on";
		dmBotLog.Debug(m_Bot.GetName(), "dmBotVoiceCenter.Say() cat=" + dmBotVoice.GetCategoryName(cat) + " lineId=" + lineId + " cooldown=" + cd);
		#endif

		return lineId;
	}
}
