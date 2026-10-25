//! dmBotState_Flanking — arc around an invisible hostile target to regain a
//! clear line-of-sight, then hand back to Shooting.
//!
//! Inherits target resolution, aim-mode selection and combat entry from
//! dmBotState_Shooting, but keeps no Aim/HitTo/Look intents — instead it drives a
//! single dmBotIntent_Flank (MOVE channel) and lowers the weapon. EXITs as soon as
//! the target becomes visible (yields back to Shooting) or disappears entirely.
class dmBotState_Flanking : dmBotState_Shooting
{
	ref dmBotIntent_Flank m_Flank;

	override dmBotCondition CreateGate()
	{
		return dmBotConditions.Flanking(true);
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		if (!m_TargetEntity || !m_TargetEntity.IsAlive()) ResolveTarget();
		if (!m_TargetEntity) return EXIT;

		dmTarget t = bot.FindTarget(m_TargetEntity);
		if (t && t.m_HasLOS)
		{
			if (m_Flank) { m_Flank.Finish(); m_Flank = null; }
			return EXIT; // цель видна — уступить Shooting
		}

		// цель вне зоны — уступить Rushing
		if (t && !bot.CanReach(t))
			return EXIT;

		EnsureFlank(bot);
		m_Flank.m_Active = true;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			pawn.RaiseWeapon(false);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Flanking: цель невидима, обход");
		#endif

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Flank) m_Flank.Finish();
		super.OnExit(to);
	}

	void EnsureFlank(dmAISurvivor bot)
	{
		if (m_Flank && (m_Flank.IsFinished() || m_Flank.IsExpired()))
			m_Flank = null;
		if (!m_Flank)
		{
			m_Flank = new dmBotIntent_Flank();
			m_Flank.m_TargetEntity = m_TargetEntity;
			m_Flank.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Flank.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Flank);
		}
	}
}
