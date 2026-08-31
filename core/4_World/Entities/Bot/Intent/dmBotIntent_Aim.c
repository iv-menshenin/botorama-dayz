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

		dmAiming aiming = pawn.GetAiming();
		if (aiming)
		{
			aiming.SetTarget(t);
			aiming.Update(pDt);
			pawn.SetAimDirection(aiming.GetAimDirection());
			bot.LookAtPoint(aiming.GetAimPosition(), dmBotLookTurn.FULL);
		}

		pawn.RaiseWeapon(true);
		if (!pawn.IsReadyToShoot())
			return;

		if (!t.m_HasLOS)
			return;

		m_FireTimer -= pDt;
		if (m_FireTimer <= 0.0)
		{
			pawn.RequestFire();
			m_FireTimer = DM_BOT_FIRE_INTERVAL;
		}
	}
}
