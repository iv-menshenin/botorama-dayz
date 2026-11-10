
enum dmHitToFail
{
	NONE,
	COOLDOWN,
	NOTARGET,
	TOOFAR,
	ANGLE,
	HASNTLOS
};

//! dmBotIntent_HitTo — strike a melee target when aligned and in reach.
//!
//! CRITICAL + PARALLEL. Only fires while the strike cooldown is zero. It bails out
//! unless the target is within m_ReachDistance, the body is turned toward it
//! (within DM_MELEE_FACE_ANGLE) and the target is in line of sight. On success it
//! requests one melee strike (RequestMeleeAttack) and restarts the cooldown.
class dmBotIntent_HitTo : dmBotIntent
{
	ref dmTarget m_Target;
	float m_ReachDistance = 1.5;
	dmHitToFail m_LastFail;

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

		m_LastFail = dmHitToFail.NONE;
		super.OnUpdate(bot, pDt);
		
		float mCd = bot.GetMeleeCooldown();
		if ( mCd > 0.0 || !m_Target || !m_Target.IsValidTarget())
		{
			#ifdef DM_BOT_DEBUG_FIGHT
			if ( m_Target && m_Target.m_Entity )
			{
				m_LastFail = dmHitToFail.COOLDOWN;
				dmBotLog.Debug(bot.GetName(), "[FSM] Fighting: удар по " + m_Target.m_Entity.GetType() + " НЕУДАЧА MeleeCooldown=" + mCd);
			} else {
				m_LastFail = dmHitToFail.NOTARGET;
				dmBotLog.Debug(bot.GetName(), "[FSM] Fighting: удар по `пустому месту` НЕУДАЧА MeleeCooldown=" + mCd);
			}
			#endif
			return;
		}

		vector targetPos = m_Target.m_Entity.GetPosition();
		vector botPos = bot.GetPosition();
		vector d = targetPos - botPos;
		float yDiff = Math.AbsFloat(d[1]); // если не проверить Y, то бот будет вас бить даже если вы на другом этаже
		d[1] = 0.0;
		float dist = d.Length() - 0.65; // 0.65 - это две толщины тела. между ботом и чеолвекоподобным существом будет ровно 0
		if (dist > m_ReachDistance || yDiff > 1.0)
		{
			m_LastFail = dmHitToFail.TOOFAR;
			#ifdef DM_BOT_DEBUG_FIGHT
			dmBotLog.Debug(bot.GetName(), "[FSM] Fighting: удар по " + m_Target.m_Entity.GetType() + " НЕУДАЧА дистанция: " + dist + " > " + m_ReachDistance);
			#endif
			return;
		}

		float yawTo = d.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float ang = dmAISurvivor.AngleDiff(yawTo, bodyYaw);
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (Math.AbsFloat(ang) > DM_MELEE_FACE_ANGLE)
		{
			m_LastFail = dmHitToFail.ANGLE;
			//! Ни в коем случае не делать слайд-доворот, это сломает бой
			//! Лучшее решение - это дать пешке завершить поворот в сторону противника, поставив удар на кулдаун
			bot.SetMeleeCooldown(0.5);
			#ifdef DM_BOT_DEBUG_FIGHT
			dmBotLog.Debug(bot.GetName(), "[FSM] Fighting: угол " + Math.AbsFloat(ang) + " > " + DM_MELEE_FACE_ANGLE + " неудача!");
			#endif
			return;
		}

		if (!m_Target.IsVisible())
		{
			m_LastFail = dmHitToFail.HASNTLOS;
			#ifdef DM_BOT_DEBUG_FIGHT
			dmBotLog.Debug(bot.GetName(), "[FSM] Fighting: удар по " + m_Target.m_Entity.GetType() + " НЕУДАЧА видимость: " + m_Target.IsVisible());
			#endif
			// бей вслепую!ы
			// return;
		}

		if (pawn)
		{
			pawn.RequestMeleeAttack(m_Target);
			bot.SetMeleeCooldown(DM_MELEE_COOLDOWN);

			#ifdef DM_BOT_DEBUG_FIGHT
			dmBotLog.Debug(bot.GetName(), "[FSM] Fighting: удар по " + m_Target.m_Entity.GetType());
			#endif
		}
	}
}
