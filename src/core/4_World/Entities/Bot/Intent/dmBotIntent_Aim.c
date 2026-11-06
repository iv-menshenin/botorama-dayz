//! dmBotIntent_Aim — aim, raise and fire at a target (one intent).
//!
//! CRITICAL + PARALLEL, LOOK channel. Every tick it feeds the target into the
//! pawn's dmAiming mechanism and enables it (the brain then ticks
//! dmAiming.OnUpdate, which computes the aim point/direction/distance and pushes
//! them into the pawn via SetAim), turns the body toward the aim point, keeps the
//! weapon raised, and — once ready and with LOS — requests a shot (RequestFire)
//! on a DM_BOT_FIRE_INTERVAL cadence.
class dmBotIntent_Aim : dmBotIntent
{
	EntityAI m_TargetEntity;
	float m_FireTimer = 0.0;
	int m_QueuedShots = 0;
	int m_SettleTicks = 0;
	float m_LowerTimer = 0.0;
	float m_HasNoLOSTimer = 0.0;

	void dmBotIntent_Aim()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.LOOK;
	}

	override string GetIntentName()
	{
		return "Aim";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.Aim");
		#endif

		super.OnUpdate(bot, pDt);

		if (!m_TargetEntity || !m_TargetEntity.IsAlive())
		{
			Finish();
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			Finish();
			return;
		}

		dmTarget t = bot.FindTarget(m_TargetEntity);
		if (!t)
		{
			Finish();
			return;
		}

		float dist = vector.Distance(bot.GetPosition(), m_TargetEntity.GetPosition());
		float chanceToRequest = 1.0;
		dmAiming aiming = pawn.GetAiming();
		if (aiming)
		{
			aiming.SetTarget(t);
			aiming.Enable();
			bot.LookAtPoint(aiming.GetAimPosition(), dmBotLookTurn.FULL);

			chanceToRequest = aiming.HitProbability() * pDt;

			//! Ближе — меньше влияния джиттера: на 50 м стреляем почти сразу,
			//! на 1000 м — HitProbability*pDt. Линейная зависимость.
			float distFactor = Math.Clamp((dist - DM_FIRE_JITTER_NEAR) / (DM_FIRE_JITTER_FAR - DM_FIRE_JITTER_NEAR), 0.0, 0.8);
			chanceToRequest = 1.0 + (chanceToRequest - 1.0) * distFactor;
		}

		//! Цель отклонилась от оси ствола — опускаем оружие (чтобы доворот корпусом
		//! был быстрее), выждав задержку, зависящую от дистанции. Угол берём от
		//! физического ствола (реальная ось пули), а не от корпуса: ствол отстаёт
		//! при довороте, пока корпус ещё разворачивается.
		vector botPos = bot.GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector toTarget = tPos - botPos;
		toTarget[1] = 0.0;
		float distToTarget = toTarget.Length();
		float targetYaw = toTarget.VectorToAngles()[0];
		float barrelYaw = pawn.GetBarrelDirection().VectorToAngles()[0];
		float angle = Math.AbsFloat(dmAISurvivor.AngleDiff(barrelYaw, targetYaw));

		if (angle > DM_SHOOT_LOWER_ANGLE)
		{
			m_LowerTimer = m_LowerTimer + pDt;
			float lowerDistFactor = Math.Clamp((distToTarget - DM_SHOOT_LOWER_NEAR_DIST) / (DM_SHOOT_LOWER_FAR_DIST - DM_SHOOT_LOWER_NEAR_DIST), 0.0, 1.0);
			float lowerDelay = DM_SHOOT_LOWER_NEAR_DELAY + (DM_SHOOT_LOWER_FAR_DELAY - DM_SHOOT_LOWER_NEAR_DELAY) * lowerDistFactor;
			if (m_LowerTimer >= lowerDelay)
				pawn.RaiseWeapon(false);
			return;
		}
		m_LowerTimer = 0.0;

		if (!pawn.IsReadyToShoot())
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug(bot.GetName(), "[FSM] BotIntent: Aim not ReadyToShoot");
			#endif
			return;
		}

		if (!t.IsVisible())
		{
			m_HasNoLOSTimer = m_HasNoLOSTimer + pDt;
			if ( m_HasNoLOSTimer > 5.0 ) pawn.RaiseWeapon( false );
			return;
		}
		m_HasNoLOSTimer = 0.0;

		//! Не стреляем, пока ось ствола отклонена от точки прицела настолько, что
		//! пуля ушла бы за силуэт (miss = 3D-угол × дальность). Порог не зависит от
		//! дальности: это смещение точки прицеливания в метрах от центра цели.
		vector aimPos = t.GetAimPosition();
		vector toAim = aimPos - botPos;
		float aimDist = toAim.Length();
		if (aimDist > 0.001)
		{
			toAim.Normalize();
			vector barrelDir = pawn.GetBarrelDirection();
			float aimAngle = Math.Acos(Math.Clamp(vector.Dot(barrelDir, toAim), -1.0, 1.0));
			float miss = aimAngle * aimDist;
			if (miss > DM_SHOOT_GATE_MISS_RADIUS)
			{
				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug(bot.GetName(), "[FSM] BotIntent: Aim off-target miss=" + miss);
				#endif
				return;
			}
		}

		m_FireTimer -= pDt;
		if (m_SettleTicks > 0)
		{
			m_SettleTicks = m_SettleTicks - 1;
			if (m_SettleTicks == 0)
			{
				DoFire(pawn, bot);
				if (aiming)
					aiming.ClearShotDispersion();
			}
			return;
		}

		float chance = Math.RandomFloat(0.0, 1.0);
		if (chance > chanceToRequest)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug(bot.GetName(), "[FSM] BotIntent: Aim low chance to hit");
			#endif
			return;
		}

		if (m_FireTimer <= 0.0)
		{
			if (aiming)
			{
				aiming.RollShotDispersion();
				m_SettleTicks = DM_AIM_SETTLE_TICKS;
			}
			else
			{
				DoFire(pawn, bot);
			}
		}
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			dmAiming aiming = pawn.GetAiming();
			if (aiming)
				aiming.Disable();
		}
	}

	private void DoFire(dmAISurvivorBase pawn, dmAISurvivor bot)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug(bot.GetName(), "[FSM] BotIntent: Aim.RequestFire");
		#endif
		Weapon_Base weapon = bot.GetWeaponInHands();
		if (m_QueuedShots <= 0 && weapon)
			m_QueuedShots = pawn.ComputeQueuedShots(weapon);
		pawn.RequestFire();
		if (m_QueuedShots > 0)
			m_QueuedShots = m_QueuedShots - 1;
		if (m_QueuedShots > 0 && weapon)
			m_FireTimer = weapon.GetReloadTime(weapon.GetCurrentMuzzle());
		else
			m_FireTimer = DM_BOT_FIRE_INTERVAL;
	}
}
