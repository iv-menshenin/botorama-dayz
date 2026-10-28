//! dmBotIntent_MeleeBlock — блок: поднять руки + отход назад; fight-logic ставит
//! SetBlock (гашение урона). Finish по полученному удару ИЛИ таймауту.
//!
//! CRITICAL + PARALLEL (MOVE). Сам блок (SetMeleeBlock + SetBlock) ставит
//! dmBotMeleeFightLogic_LightHeavy.HandleFightLogic каждый кадр по условию
//! «IsRaised() && желаемое движение назад» — интенту достаточно поднять оружие
//! (RaiseWeapon) и командовать движение назад (IntentMove 180°), а корпус к врагу
//! держит HoldLook (FULL) из состояния. Гашение урона читает скриптовый SetBlock
//! в EvaluateHit_Player по углу разворота к атакующему.
class dmBotIntent_MeleeBlock : dmBotIntent
{
	bool m_Armed;
	float m_Elapsed;

	void dmBotIntent_MeleeBlock()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "MeleeBlock";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn) { Finish(); return; }

		if (!m_Armed)
		{
			pawn.ExpectHit();   // вооружаем латч «получил удар»
			m_Armed = true;
		}
		pawn.RaiseWeapon(true);
		bot.IntentMove(180.0, DM_MELEE_BLOCK_SPEED);   // назад → fight-logic ставит SetBlock
		m_Elapsed += pDt;
		if (pawn.ReceivedHit() || m_Elapsed > DM_MELEE_BLOCK_TIMEOUT)
			Finish();
	}
}
