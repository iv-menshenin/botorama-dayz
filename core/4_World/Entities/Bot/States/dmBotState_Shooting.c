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
	float m_PreferredSpeed;

	ref dmBotIntent_Aim m_Aim;
	ref dmBotIntent_HitTo m_HitTo;
	ref dmBotIntent_HoldLook m_Look;
	ref dmBotIntent_Flank m_Flank;

	float m_InFlanking;
	float m_RetargetTimer;
	float m_Elapsed;
	bool m_NoFirearm;

	const float HIT_BUTTSTCK_RANGE = 1.5;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		super.OnEntry(from);

		m_TargetEntity = null;
		m_Target = null;
		m_Aim = null;
		m_Flank = null;
		m_RetargetTimer = 0.0;
		m_Elapsed = 0.0;
		m_NoFirearm = false;

		ResolveTarget();

		dmAISurvivor bot = GetOwner();
		m_PreferredSpeed = bot.GetPreferredSpeed();
		bot.SetPreferredSpeed(3.0);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn && m_TargetEntity)
		{
			vector botPos = bot.GetPosition();
			vector tPos = m_TargetEntity.GetPosition();
			vector d = tPos - botPos;
			d[1] = 0.0;
			float dist = d.Length();

			Weapon_Base w = bot.SelectFirearmForRange(dist);
			if (w)
			{
				if (w != bot.GetWeaponInHands())
					dmLoot.TakeToHands(pawn, w);

				pawn.SetAimMode(SelectAimMode());
				pawn.RaiseWeapon(true);

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Shooting: огнестрел " + w.GetType() + " dist=" + dist);
				#endif
			}
			else
			{
				m_NoFirearm = true;

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Shooting: нет заряженного огнестрела — EXIT");
				#endif
			}
		}

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Shooting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !m_TargetEntity)
			return EXIT;

		if (m_NoFirearm)
		{
			m_CooldownGameTime = GetGame().GetTickTime() + 15.0;
			return EXIT;
		}

		if (!m_TargetEntity || !m_TargetEntity.IsAlive())
		{
			ResolveTarget();
			if (!m_TargetEntity)
				return EXIT;
		}

		EnsureLook();
		EnsureHitTo(bot);
		EnsureFlank(bot);

		vector botPos = bot.GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = tPos - botPos;
		d[1] = 0.0;
		float dist = d.Length();
		m_HitTo.m_Active = (dist <= HIT_BUTTSTCK_RANGE);
		m_Look.m_Active = (dist <= HIT_BUTTSTCK_RANGE);

		//! Фланг: цель враждебна, но не видна и далеко — обходим укрытие по дуге.
		dmTarget t = bot.FindTarget(m_TargetEntity);
		bool flankActive = false;
		if (t && !t.m_HasLOS && t.m_Threat >= DM_ATTACK_THREAT_THRESHOLD && dist > DM_FLANK_MIN_DIST)
		{
			flankActive = true;
			m_InFlanking += pDt;
		} else {
			m_InFlanking = 0.0;
		}
		m_Flank.m_Active = flankActive;
		if (m_Aim) m_Aim.m_Active = !flankActive;

		//! Если бот давно не стрелял (напр. цель вне досягаемости/нет LOS) —
		//! перевыставить предпочтительный режим огня на оружии.
		if (pawn.GetTimeSinceLastShot() > 10.0)
			pawn.RefreshPreferredFireMode();

		m_Elapsed += pDt;

		if (!m_TargetEntity || !m_TargetEntity.IsAlive()) ResolveTarget();
		if (!m_TargetEntity) return EXIT;

		m_RetargetTimer += pDt;
		if (m_RetargetTimer >= DM_FIGHT_RETARGET_INTERVAL)
		{
			m_RetargetTimer = 0.0;
			ResolveTarget();
		}
		if (!m_TargetEntity)
			return EXIT;

		if ((bot.HasNoAmmo() || bot.CheckNeedsChamber()) && dist > HIT_BUTTSTCK_RANGE)
		{
			if (!pawn.ReloadWeaponAI())
			{
				m_CooldownGameTime = GetGame().GetTickTime() + 15.0;
				return EXIT;
			}
			return CONTINUE;
		}

		#ifdef DM_BOT_DEBUG_FSM
		if (t)
			dmBotLog.Debug("[FSM] Shooting: flankActive=" + flankActive + " m_InFlanking=" + m_InFlanking + " dist=" + dist + " m_HasLOS=" + t.m_HasLOS);
		#endif

		if ( pawn.IsWeaponReady() && dist > HIT_BUTTSTCK_RANGE && !(flankActive && m_InFlanking > DM_FLANK_LOW_WEAPON_TIMING))
		{
			pawn.SetAimMode(SelectAimMode());
			pawn.RaiseWeapon(true);
			EnsureAim(bot);
		} else {
			pawn.RaiseWeapon(false);
			if (m_Aim) m_Aim.m_Active = !flankActive;
		}

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Shooting.exit");
		#endif

		dmAISurvivor bot = GetOwner();
		bot.SetPreferredSpeed(m_PreferredSpeed);

		if ( m_HitTo ) m_HitTo.Finish();
		if ( m_Aim ) m_Aim.Finish();
		if ( m_Look ) m_Look.Finish();
		if ( m_Flank ) m_Flank.Finish();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
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
			if (m_HitTo) { m_HitTo.Finish(); m_HitTo = null; }
			if (m_Look) { m_Look.Finish(); m_Look = null; }
			if (m_Flank) { m_Flank.Finish(); m_Flank = null; }
			m_Elapsed = 0.0;
			m_RetargetTimer = 0.0;

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Shooting: цель сменилась, интент сброшен");
			#endif
		}

		m_Target = t;
		m_TargetEntity = newEntity;
	}

	void EnsureHitTo(dmAISurvivor bot)
	{
		if (m_HitTo && (m_HitTo.IsFinished() || m_HitTo.IsExpired()))
			m_HitTo = null;
		if (!m_HitTo)
		{
			m_HitTo = new dmBotIntent_HitTo();
			m_HitTo.m_TargetEntity = m_TargetEntity;
			m_HitTo.m_ReachDistance = HIT_BUTTSTCK_RANGE;
			m_HitTo.m_Priority = dmBotIntentPriority.CRITICAL;
			m_HitTo.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_HitTo);
		}
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
}
