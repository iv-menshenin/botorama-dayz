//! dmBotIntent_MeleeEvade — уворот (нативный ролл) в сторону, противоположную врагу.
//! Finish по завершении ролла (IsMeleeEvade true→false).
//!
//! CRITICAL + PARALLEL (MOVE). Запрос ролла — один раз через RequestMeleeEvade,
//! потребляется dmBotMeleeFightLogic_LightHeavy.HandleFightLogic → StartMeleeEvadeA.
//! Направление — в противоположную от врага сторону (±90° относительно корпуса):
//! bearing > 0 (враг справа) → ролл влево (-90), иначе вправо (+90).
class dmBotIntent_MeleeEvade : dmBotIntent
{
	EntityAI m_TargetEntity;
	bool m_Requested;
	bool m_Started;

	void dmBotIntent_MeleeEvade()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "MeleeEvade";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn) { Finish(); return; }
		if (!m_TargetEntity) { Finish(); return; }

		if (!m_Requested)
		{
			vector botPos = bot.GetPosition();
			vector enemyPos = m_TargetEntity.GetPosition();
			vector toEnemy = enemyPos - botPos;
			toEnemy[1] = 0.0;
			float yawToEnemy = toEnemy.VectorToAngles()[0];
			float bodyYaw = bot.GetOrientation()[0];
			float bearing = dmAISurvivor.AngleDiff(yawToEnemy, bodyYaw); // >0 = враг справа
			float evadeAngle = 90.0;                                     // в противоположную сторону
			if (bearing > 0.0)
				evadeAngle = -90.0;
			pawn.RequestMeleeEvade(evadeAngle);
			m_Requested = true;
		}

		if (!m_Started && pawn.IsMeleeEvade())
			m_Started = true;
		if (m_Started && !pawn.IsMeleeEvade())
			Finish();   // ролл завершён
	}
}
