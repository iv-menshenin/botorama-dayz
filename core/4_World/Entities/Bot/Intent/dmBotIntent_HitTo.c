//! dmBotIntent_HitTo — strike a melee target when aligned and in reach.
//!
//! CRITICAL + PARALLEL. Only fires while the strike cooldown is zero. It bails out
//! unless the target is within m_ReachDistance, the body is turned toward it
//! (within DM_MELEE_FACE_ANGLE) and the target is in line of sight. On success it
//! requests one melee strike (RequestMeleeAttack) and restarts the cooldown.
class dmBotIntent_HitTo : dmBotIntent
{
	EntityAI m_TargetEntity;
	float m_ReachDistance = 1.5;

	void dmBotIntent_HitTo()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.ATTACK;
	}

	override string GetIntentName()
	{
		return "HitTo";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.HitTo");
		#endif

		super.OnUpdate(bot, pDt);
		float mCd = bot.GetMeleeCooldown();
		if ( mCd > 0.0 || !m_TargetEntity)
		{
			#ifdef DM_BOT_DEBUG_FSM
			if ( m_TargetEntity )
			{
				dmBotLog.Debug("[FSM] Fighting: удар по " + m_TargetEntity.GetType() + " НЕУДАЧА MeleeCooldown=" + mCd);
			} else {
				dmBotLog.Debug("[FSM] Fighting: удар по `пустому месту` НЕУДАЧА MeleeCooldown=" + mCd);
			}
			#endif
			return;
		}

		vector targetPos = m_TargetEntity.GetPosition();
		vector botPos = bot.GetPosition();
		vector d = targetPos - botPos;
		d[1] = 0.0;
		float dist = d.Length();
		if (dist > m_ReachDistance)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Fighting: удар по " + m_TargetEntity.GetType() + " НЕУДАЧА дистанция: " + dist + " > " + m_ReachDistance);
			#endif
			return;
		}

		float yawTo = d.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float ang = dmAISurvivor.AngleDiff(yawTo, bodyYaw);
		if (Math.AbsFloat(ang) > DM_MELEE_FACE_ANGLE)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Fighting: удар по " + m_TargetEntity.GetType() + " НЕУДАЧА угол: " + Math.AbsFloat(ang) + " > " + DM_MELEE_FACE_ANGLE);
			#endif
			return;
		}

		dmTarget t = bot.FindTarget(m_TargetEntity);
		if (!t || !t.m_HasLOS)
		{
			#ifdef DM_BOT_DEBUG_FSM
			if ( t )
			{
				dmBotLog.Debug("[FSM] Fighting: удар по " + m_TargetEntity.GetType() + " НЕУДАЧА видимость: " + t.m_HasLOS);
			} else {
				dmBotLog.Debug("[FSM] Fighting: удар по " + m_TargetEntity.GetType() + " НЕУДАЧА нет цели [FindTarget]");
			}
			#endif
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			pawn.RequestMeleeAttack(m_TargetEntity);
			bot.SetMeleeCooldown(DM_MELEE_COOLDOWN);

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Fighting: удар по " + m_TargetEntity.GetType());
			#endif
		}
	}
}
