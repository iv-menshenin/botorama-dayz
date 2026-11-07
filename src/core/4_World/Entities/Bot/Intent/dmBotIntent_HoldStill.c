//! dmBotIntent_HoldStill — персональный «стоп»: выигрывает MOVE-канал и не шлёт
//! движение (OnUpdate пуст) → авто-стоп в HandleOnMove гасит движение. Пара к HoldLook
//! для реакции «остановиться и смотреть».
class dmBotIntent_HoldStill : dmBotIntent
{
	void dmBotIntent_HoldStill()
	{
		m_Manage = dmBotIntentsChannel.MOVE;
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
	}

	override string GetIntentName()
	{
		return "HoldStill";
	}

	//! OnUpdate по умолчанию пуст (наследуется) — держит позицию.
};
