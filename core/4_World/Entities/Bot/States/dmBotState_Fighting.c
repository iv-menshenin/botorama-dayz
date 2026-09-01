//! dmBotState_Fighting — reactive melee engagement (thin coordinator).
//!
//! PREEMPTIVE "melee fight" state (no raised stance — strikes come from ERECT).
//! Unlike the old linear flow, the state is now a thin coordinator that resolves a
//! hostile target and keeps three CRITICAL intents alive depending on range and
//! the strike cooldown:
//!   - out of reach       -> Approach (move toward the target);
//!   - in reach, cooldown -> Evasion (strafe while the strike recharges);
//!   - in reach, ready    -> HitTo (face + request a melee strike).
//! A HoldLook (FULL) intent keeps the body turned toward the target the whole time.
//! Target resolution runs on entry and every DM_FIGHT_RETARGET_INTERVAL seconds,
//! always taking the nearest hostile from dmAISurvivor.GetHostileTarget().
class dmBotState_Fighting : dmBotState
{
	EntityAI m_TargetEntity;
	ref dmTarget m_Target;
	ref dmBotIntent_Approach m_Approach;
	ref dmBotIntent_Evasion m_Evasion;
	ref dmBotIntent_HitTo m_HitTo;
	ref dmBotIntent_HoldLook m_Look;
	float m_RetargetTimer;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		m_TargetEntity = null;
		m_Target = null;
		m_Approach = null;
		m_Evasion = null;
		m_HitTo = null;
		m_Look = null;
		m_RetargetTimer = 0.0;

		dmAISurvivor bot = GetOwner();
		bot.SetMeleeCooldown(0.0);

		ResolveTarget();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Fighting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		if (!m_TargetEntity || !m_TargetEntity.IsAlive())
			ResolveTarget();
		if (!m_TargetEntity)
			return EXIT;

		m_RetargetTimer += pDt;
		if (m_RetargetTimer >= DM_FIGHT_RETARGET_INTERVAL)
		{
			m_RetargetTimer = 0.0;
			ResolveTarget();
		}
		if (!m_TargetEntity)
			return EXIT;

		//! Tick the strike cooldown (bot-level, read by HitTo/Evasion).
		float cd = bot.GetMeleeCooldown() - pDt;
		if (cd < 0.0)
			cd = 0.0;
		bot.SetMeleeCooldown(cd);

		vector botPos = bot.GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = tPos - botPos;
		d[1] = 0.0;
		float distSq = d.LengthSq();
		float reach = GetMeleeReach(bot);
		float reachSq = reach * reach;

		EnsureLook();
		EnsureApproach(bot);
		EnsureEvasion(bot);
		EnsureHitTo(bot);

		m_HitTo.m_Active = (distSq <= reachSq && bot.GetMeleeCooldown() == 0.0);
		m_Approach.m_Active = (m_HitTo.m_LastFail == dmHitToFail.TOOFAR) || (distSq > reachSq) || (bot.GetMeleeCooldown() == 0.0 && distSq > (reachSq * 0.9)); // a small gap
		m_Evasion.m_Active = (bot.GetMeleeCooldown() > 0.0) && !m_Approach.m_Active;

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Fighting.exit");
		#endif

		if ( m_Approach ) m_Approach.Finish();
		if ( m_HitTo ) m_HitTo.Finish();
		if ( m_Evasion ) m_Evasion.Finish();
		if ( m_Look ) m_Look.Finish();
	}

	void ResolveTarget()
	{
		dmAISurvivor bot = GetOwner();
		dmTarget t = bot.GetHostileTarget();
		EntityAI newEntity = null;
		if (t)
			newEntity = t.m_Entity;

		if (newEntity != m_TargetEntity)
		{
			//! Цель сменилась (или исчезла) — сбросить интенты, чтобы они
			//! пересоздались под новую цель (иначе бьют/смотрят в старую).
			if (m_Approach) { m_Approach.Finish(); m_Approach = null; }
			if (m_Evasion) { m_Evasion.Finish(); m_Evasion = null; }
			if (m_HitTo) { m_HitTo.Finish(); m_HitTo = null; }
			if (m_Look) { m_Look.Finish(); m_Look = null; }

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Fighting: цель сменилась, интенты сброшены");
			#endif
		}

		m_Target = t;
		m_TargetEntity = newEntity;
	}

	void EnsureLook()
	{
		if (m_Look && (m_Look.IsFinished() || m_Look.IsExpired()))
			m_Look = null;
		if (!m_Look)
		{
			m_Look = new dmBotIntent_HoldLook();
			m_Look.m_Entity = m_TargetEntity;
			m_Look.m_Turn = dmBotLookTurn.FULL;
			m_Look.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			GetOwner().AddFSMIntent(m_Look);
		}
	}

	void EnsureApproach(dmAISurvivor bot)
	{
		if (m_Approach && (m_Approach.IsFinished() || m_Approach.IsExpired()))
			m_Approach = null;
		if (!m_Approach)
		{
			m_Approach = new dmBotIntent_Approach();
			m_Approach.m_TargetEntity = m_TargetEntity;
			m_Approach.m_ReachDistance = GetMeleeReach(bot);
			m_Approach.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Approach.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Approach);
		}
	}

	void EnsureEvasion(dmAISurvivor bot)
	{
		if (m_Evasion && (m_Evasion.IsFinished() || m_Evasion.IsExpired()))
			m_Evasion = null;
		if (!m_Evasion)
		{
			m_Evasion = new dmBotIntent_Evasion();
			m_Evasion.m_TargetEntity = m_TargetEntity;
			m_Evasion.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Evasion.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Evasion);
		}
	}

	void EnsureHitTo(dmAISurvivor bot)
	{
		if (m_HitTo && (m_HitTo.IsFinished() || m_HitTo.IsExpired()))
			m_HitTo = null;
		if (!m_HitTo)
		{
			m_HitTo = new dmBotIntent_HitTo();
			m_HitTo.m_TargetEntity = m_TargetEntity;
			m_HitTo.m_ReachDistance = GetMeleeReach(bot);
			m_HitTo.m_Priority = dmBotIntentPriority.CRITICAL;
			m_HitTo.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_HitTo);
		}
	}

	//! Weapon reach (melee combat GetRange()) with a static fallback.
	float GetMeleeReach(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			dmBotMeleeCombat mc = dmBotMeleeCombat.Cast(pawn.GetMeleeCombat());
			if (mc)
				return mc.GetReach();
		}

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Fighting: нет dmBotMeleeCombat - fallback");
		#endif
		return DM_MELEE_REACH;
	}
}
