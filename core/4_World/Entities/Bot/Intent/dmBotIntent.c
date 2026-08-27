//! dmBotIntent — atomic, concurrent "desire" of the bot (intent-pool layer).
//!
//! Intents sit between the FSM (what to do) and the brain primitives (how). They
//! are concurrent: several may be active at once, and arbitration decides which
//! ones control the body (look / movement) each tick. Priority/concurrency/deadline
//! are plain public config fields.

enum dmBotIntentPriority
{
	IDLE,       // если больше нечего делать
	DESIRABLE,  // желательно
	CRITICAL    // критично (угроза жизни / приказ)
};

enum dmBotIntentConcurrency
{
	PARALLEL,   // может выполняться параллельно с другими
	EXCLUSIVE   // должно выполняться в свою очередь (глушит остальное)
};

enum dmBotLookTurn
{
	NONE,   // только голова (не глянет за спину)
	AUTO,   // через плечо, если цель за пределами головы (текущее поведение)
	FULL    // полностью развернуться к цели
};

class dmBotIntent
{
	dmBotIntentPriority m_Priority = dmBotIntentPriority.IDLE;
	dmBotIntentConcurrency m_Concurrency = dmBotIntentConcurrency.PARALLEL;
	float m_Deadline = -1.0;   // секунды; -1 = без дедлайна

	float m_Age = 0.0;         // runtime, тикает пул
	bool m_Finished = false;
	bool m_Cancelled = false;
	bool m_Failed = false;

	void OnStart(dmAISurvivor bot) {}
	void OnUpdate(dmAISurvivor bot, float pDt) {}
	void OnCancel(dmAISurvivor bot) {}

	//! Вызвать из OnUpdate, когда условие выполнения достигнуто.
	void Finish()
	{
		m_Finished = true;
	}

	//! Вызвать из OnUpdate, когда выполнение невозможно (цель недостижима и т.п.).
	//! Завершает интент с пометкой неудачи (пул его удалит).
	void Fail()
	{
		m_Failed = true;
		m_Finished = true;
	}

	void TickAge(float pDt)
	{
		m_Age += pDt;
	}

	bool IsFinished()
	{
		return m_Finished || m_Cancelled;
	}

	bool IsFailed()
	{
		return m_Failed;
	}

	bool IsExpired()
	{
		if (m_Deadline >= 0.0)
			return m_Age > m_Deadline;
		return m_Age > DM_INTENT_MAX_AGE; // нет дедлайна -> автодедлайн (безопасность)
	}

	dmBotIntentPriority GetPriority()
	{
		return m_Priority;
	}

	dmBotIntentConcurrency GetConcurrency()
	{
		return m_Concurrency;
	}
}
