//! dmBotIntent_MeleeStrike — один нативный мили-удар (request → start → done).
//!
//! CRITICAL + PARALLEL (ATTACK). Запрашивает удар через RequestMeleeAttack один
//! раз, ждёт старта нативного мили (IsMeleeStriking) и завершается (Finish) по его
//! концу. Решение «когда бить» (дистанция/угол) — за FSM; интент только исполняет.
class dmBotIntent_MeleeStrike : dmBotIntent
{
	EntityAI m_TargetEntity;
	bool m_Requested;
	bool m_Started;

	void dmBotIntent_MeleeStrike()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.ATTACK;
	}

	override string GetIntentName()
	{
		return "MeleeStrike";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn) { Finish(); return; }

		if (!m_Requested)
		{
			pawn.RequestMeleeAttack(m_TargetEntity);
			m_Requested = true;
		}
		if (!m_Started && pawn.IsMeleeStriking())
			m_Started = true;
		if (m_Started && !pawn.IsMeleeStriking())
			Finish();   // удар завершён
	}
}
