//! dmBotVoiceCenter — центр воспроизведения реплик: решает, КОГДА бот заговорит,
//! держит кулдаун молчания (10 мин) и выбирает случайную реплику из категории.
class dmBotVoiceCenter
{
	private ref dmAISurvivor m_Bot;
	private float m_LastSpeechTime;

	void dmBotVoiceCenter(dmAISurvivor bot)
	{
		m_Bot = bot;
		m_LastSpeechTime = -DM_VOICE_COOLDOWN; // первый SayCategory всегда проходит
	}

	//! Попытаться сказать случайную реплику из категории. Возвращает lineId (>=1),
	//! если заговорил, или -1 если кулдаун молчания / пустая категория.
	int SayCategory(dmVoiceCategory cat)
	{
		float now = GetGame().GetTickTime();
		if (now - m_LastSpeechTime < DM_VOICE_COOLDOWN)
			return -1;

		int count = dmBotVoice.GetCategoryCount(cat);
		if (count <= 0)
			return -1;

		int index = Math.RandomIntInclusive(1, count);
		int lineId = cat * 100 + index;

		m_LastSpeechTime = now;

		dmBotIntent_Voice intent = new dmBotIntent_Voice();
		intent.m_LineId = lineId;
		m_Bot.AddPersonalityIntent(intent);

		#ifdef DM_BOT_DEBUG_VOICE
		dmBotLog.Debug("dmBotVoiceCenter.SayCategory() cat=" + dmBotVoice.GetCategoryName(cat) + " lineId=" + lineId);
		#endif

		return lineId;
	}

	//! Тест-хук: сбросить кулдаун молчания (автотест не ждёт 10 минут).
	void ResetCooldown()
	{
		m_LastSpeechTime = -DM_VOICE_COOLDOWN;
	}
}
