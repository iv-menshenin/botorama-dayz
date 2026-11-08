//! dmBotTest_Voice — автотест центра воспроизведения реплик: покатегорийный кулдаун,
//! случайный выбор из категории, Say без кулдауна. Без проверки аудио (только логика).
class dmBotTest_Voice : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
	}

	override string GetSummary()
	{
		return "Голос: Say/SayNoCooldown/кулдаун категории. Ожидание: Say вернёт lineId из GREETING, повторный Say — -1 (кулдаун 60с), SayNoCooldown — снова lineId, следующий Say — снова -1.";
	}

	override float GetInterval() { return 1.0; }
	override float GetDuration() { return 15.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot)
			return "FAIL: бот исчез из мира";

		// Фаза 1: первый Say — заговорил, lineId из GREETING, index в диапазоне.
		int lineId = m_Bot.Say(dmVoiceCategory.GREETING);
		if (lineId < 1)
			return "FAIL: Say не заговорил (lineId=" + lineId + ")";
		if (lineId / 100 != dmVoiceCategory.GREETING)
			return "FAIL: lineId вне GREETING (lineId=" + lineId + ")";
		int index = lineId % 100;
		if (index < 1 || index > dmBotVoice.GetCategoryCount(dmVoiceCategory.GREETING))
			return "FAIL: index вне диапазона (index=" + index + ")";

		// Фаза 2: кулдаун категории подавляет повторный Say.
		int second = m_Bot.Say(dmVoiceCategory.GREETING);
		if (second != -1)
			return "FAIL: кулдаун не сработал (second=" + second + ")";

		// Фаза 3: SayNoCooldown проходит даже в кулдаун.
		int third = m_Bot.SayNoCooldown(dmVoiceCategory.GREETING);
		if (third < 1)
			return "FAIL: SayNoCooldown не заговорил (third=" + third + ")";

		// Фаза 4: SayNoCooldown обновил таймер категории — Say снова под кулдауном.
		int fourth = m_Bot.Say(dmVoiceCategory.GREETING);
		if (fourth != -1)
			return "FAIL: SayNoCooldown не обновил таймер категории (fourth=" + fourth + ")";

		return "PASS: Say/SayNoCooldown/кулдаун категории работают (lineId=" + lineId + ")";
	}
}
