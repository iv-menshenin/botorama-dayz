//! dmBotState_Shooting — ranged engagement (thin coordinator).
//!
//! PREEMPTIVE "shoot" state. The state no longer computes aim or fires inline:
//! it resolves a hostile target (dmAISurvivor.GetHostileTarget()), picks the
//! aim mode (HIP within DM_AIM_ADS_DISTANCE for DM_AIM_HIP_GRACE seconds, then
//! ADS; ADS immediately beyond), keeps the weapon raised, and keeps a single
//! dmBotIntent_Aim intent alive — which does the actual aiming (dmAiming),
//! body turn, raise and fire. Target resolution runs on entry and every
//! DM_FIGHT_RETARGET_INTERVAL seconds, always taking the nearest hostile.
//! EXITs when no hostile target remains or the weapon runs out of ammo.
class dmBotState_Shooting : dmBotState
{
	EntityAI m_TargetEntity;
	ref dmTarget m_Target;
	ref dmBotIntent_Aim m_Aim;
	float m_RetargetTimer;
	float m_Elapsed;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		m_TargetEntity = null;
		m_Target = null;
		m_Aim = null;
		m_RetargetTimer = 0.0;
		m_Elapsed = 0.0;

		ResolveTarget();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetOwner().GetPawn());
		if (pawn)
		{
			pawn.SetAimMode(SelectAimMode());
			pawn.RaiseWeapon(true);
		}

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Shooting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return EXIT;

		m_Elapsed += pDt;

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

		if (bot.HasNoAmmo())
		{
			if (!pawn.ReloadWeaponAI())
				return EXIT;
			return CONTINUE;
		}

		pawn.SetAimMode(SelectAimMode());
		pawn.RaiseWeapon(true);

		EnsureAim(bot);

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Shooting.exit");
		#endif

		if (m_Aim)
			m_Aim.Finish();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetOwner().GetPawn());
		if (pawn)
			pawn.RaiseWeapon(false);
	}

	//! HIP вблизи в течение DM_AIM_HIP_GRACE, затем ADS; дальше DM_AIM_ADS_DISTANCE — сразу ADS.
	dmBotAimMode SelectAimMode()
	{
		vector botPos = GetOwner().GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = tPos - botPos;
		d[1] = 0.0;
		float dist = d.Length();
		if (dist > DM_AIM_ADS_DISTANCE)
			return dmBotAimMode.ADS;
		if (m_Elapsed < DM_AIM_HIP_GRACE)
			return dmBotAimMode.HIP;
		return dmBotAimMode.ADS;
	}

	void EnsureAim(dmAISurvivor bot)
	{
		if (m_Aim && (m_Aim.IsFinished() || m_Aim.IsExpired()))
			m_Aim = null;
		if (!m_Aim)
		{
			m_Aim = new dmBotIntent_Aim();
			m_Aim.m_TargetEntity = m_TargetEntity;
			m_Aim.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Aim.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Aim);
		}
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
			if (m_Aim) { m_Aim.Finish(); m_Aim = null; }
			m_Elapsed = 0.0;

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Shooting: цель сменилась, интент сброшен");
			#endif
		}

		m_Target = t;
		m_TargetEntity = newEntity;
	}
}
