//! dmBotTest_Voice — автотест центра воспроизведения реплик: кулдаун молчания,
//! случайный выбор из категории, сброс кулдауна. Без проверки аудио (только логика).
class dmBotTest_Voice : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
	}

	override string GetSummary()
	{
		return "Голос: категоризатор/выбиратор/кулдаун. Ожидание: SayCategory вернёт lineId из GREETING, повторный — -1 (кулдаун), после ResetCooldown — снова lineId.";
	}

	override float GetInterval() { return 1.0; }
	override float GetDuration() { return 15.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot)
			return "FAIL: бот исчез из мира";

		dmBotVoiceCenter center = m_Bot.GetVoiceCenter();
		if (!center)
			return "FAIL: нет dmBotVoiceCenter у мозга";

		// Фаза 1: после сброса — заговорил, lineId из GREETING, index в диапазоне.
		center.ResetCooldown();
		int lineId = m_Bot.SayCategory(dmVoiceCategory.GREETING);
		if (lineId < 1)
			return "FAIL: SayCategory не заговорил (lineId=" + lineId + ")";
		if (lineId / 100 != dmVoiceCategory.GREETING)
			return "FAIL: lineId вне GREETING (lineId=" + lineId + ")";
		int index = lineId % 100;
		if (index < 1 || index > dmBotVoice.GetCategoryCount(dmVoiceCategory.GREETING))
			return "FAIL: index вне диапазона (index=" + index + ")";

		// Фаза 2: кулдаун подавляет повторный вызов.
		int second = m_Bot.SayCategory(dmVoiceCategory.GREETING);
		if (second != -1)
			return "FAIL: кулдаун не сработал (second=" + second + ")";

		// Фаза 3: сброс кулдауна — снова заговорил.
		center.ResetCooldown();
		int third = m_Bot.SayCategory(dmVoiceCategory.GREETING);
		if (third < 1)
			return "FAIL: после ResetCooldown не заговорил";

		return "PASS: категоризатор/выбиратор/кулдаун работают (lineId=" + lineId + ")";
	}
}
