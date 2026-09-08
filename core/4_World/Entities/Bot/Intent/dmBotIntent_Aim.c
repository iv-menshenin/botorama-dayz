//! dmBotIntent_Aim — aim, raise and fire at a target (one intent).
//!
//! CRITICAL + PARALLEL, LOOK channel. Every tick it feeds the target into the
//! pawn's dmAiming accuracy model, turns the body toward the aim point, pushes
//! the dispersed shot direction into the pawn (SetAimDirection), keeps the weapon
//! raised, and — once ready and with LOS — requests a shot (RequestFire) on a
//! DM_BOT_FIRE_INTERVAL cadence.
class dmBotIntent_Aim : dmBotIntent
{
	EntityAI m_TargetEntity;
	float m_FireTimer = 0.0;
	int m_QueuedShots = 0;
	int m_SettleTicks = 0;

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

		float chanceToRequest = 1.0;
		dmAiming aiming = pawn.GetAiming();
		if (aiming)
		{
			aiming.SetTarget(t);
			aiming.Update(pDt);
			pawn.SetAimDirection(aiming.GetAimDirection());
			bot.LookAtPoint(aiming.GetAimPosition(), dmBotLookTurn.FULL);

			chanceToRequest = aiming.HitProbability() * pDt;
		}

		pawn.RaiseWeapon(true);
		if (!pawn.IsReadyToShoot())
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] BotIntent: Aim not ReadyToShoot");
			#endif
			return;
		}

		if (!t.m_HasLOS)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] BotIntent: Aim has no LOS");
			#endif
			return;
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
			dmBotLog.Debug("[FSM] BotIntent: Aim low chance to hit");
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

	private void DoFire(dmAISurvivorBase pawn, dmAISurvivor bot)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] BotIntent: Aim.RequestFire");
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
