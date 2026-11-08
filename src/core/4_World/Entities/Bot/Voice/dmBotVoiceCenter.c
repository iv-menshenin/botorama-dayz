//! dmBotVoiceCenter — центр воспроизведения реплик: решает, КОГДА бот заговорит,
//! держит покатегорийный кулдаун молчания и выбирает случайную реплику из категории.
class dmBotVoiceCenter
{
	private ref dmAISurvivor m_Bot;
	private ref map<dmVoiceCategory, float> m_LastSpokeAt;
	private float m_NextAmbientChatAt = 0.0; // 0 = не инициализирован; момент следующего фонового трёпа

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

	//! Фоновый трёп (IDLE/PATROL): если наступил момент — сказать случайную реплику
	//! категории и перезапланировать следующий момент (15 мин + случайные 0..15 мин).
	void TryAmbientChat(dmVoiceCategory cat)
	{
		float now = GetGame().GetTickTime();
		if (m_NextAmbientChatAt <= 0.0)
			m_NextAmbientChatAt = now + DM_VOICE_COOLDOWN_DEFAULT + Math.RandomIntInclusive(0, (int)DM_VOICE_COOLDOWN_DEFAULT);
		if (now < m_NextAmbientChatAt)
			return;
		Say(cat);
		m_NextAmbientChatAt = now + DM_VOICE_COOLDOWN_DEFAULT + Math.RandomIntInclusive(0, (int)DM_VOICE_COOLDOWN_DEFAULT);
	}

	//! Может ли бот говорить: не вырублен и нет болезни от человечины (Kuru/прион).
	private bool CanSpeak()
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Bot.GetPawn());
		if (!pawn)
			return false;
		if (pawn.IsUnconsciousStateOnly())
			return false;
		ModifiersManager mngr = pawn.GetModifiersManager();
		if (mngr && mngr.IsModifierActive(eModifiers.MDF_BRAIN))
			return false;
		return true;
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

		//! Гейт состояния: нельзя говорить вырубленным/с болезнью.
		if (!CanSpeak())
			return -1;

		int index = Math.RandomIntInclusive(from, to);
		int lineId = cat * 100 + index;

		m_LastSpokeAt.Set(cat, now);

		//! Реакция: реальная реплика — через 0.5–1.5 с (время реакции).
		int delayMs = (int)(Math.RandomFloat(DM_VOICE_REACTION_DELAY_MIN, DM_VOICE_REACTION_DELAY_MAX) * 1000.0);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CreateVoiceIntent, delayMs, false, lineId);

		#ifdef DM_BOT_DEBUG_VOICE
		string cd = "off";
		if (respectCooldown)
			cd = "on";
		dmBotLog.Debug(m_Bot.GetName(), "dmBotVoiceCenter.Say() cat=" + dmBotVoice.GetCategoryName(cat) + " lineId=" + lineId + " cooldown=" + cd);
		#endif

		return lineId;
	}

	//! Отложенное создание интента реплики (после оттяжки реакции).
	private void CreateVoiceIntent(int lineId)
	{
		dmBotIntent_Voice intent = new dmBotIntent_Voice();
		intent.m_LineId = lineId;
		m_Bot.AddPersonalityIntent(intent);
	}
}
