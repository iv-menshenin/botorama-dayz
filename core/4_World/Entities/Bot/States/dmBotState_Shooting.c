//! dmBotState_Shooting — ranged engagement: raise weapon, aim, fire.
//!
//! PREEMPTIVE linear state (no intents): raise the weapon, wait for the raise to
//! complete, aim at the hostile target, and fire on a cooldown while the target
//! has LOS. The target is the reactive threat model result
//! (dmAISurvivor.GetHostileTarget()): the nearest living hostile. Re-resolves on
//! entry, when the target dies, and every DM_FIGHT_RETARGET_INTERVAL seconds;
//! EXITs when no hostile target remains or the weapon runs out of ammo.
class dmBotState_Shooting : dmBotState
{
	EntityAI m_TargetEntity;
	ref dmTarget m_Target;
	float m_FireTimer;
	float m_RetargetTimer;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		m_TargetEntity = null;
		m_Target = null;
		m_FireTimer = 0.0;
		m_RetargetTimer = 0.0;
		ResolveTarget();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetOwner().GetPawn());
		if (pawn)
			pawn.RaiseWeapon(true);

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

		pawn.RaiseWeapon(true);
		if (!pawn.IsWeaponRaiseCompleted())
			return CONTINUE;

		vector targetPos = m_TargetEntity.GetPosition();
		bot.LookAtPoint(targetPos + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.FULL);
		pawn.SetAimTarget(m_TargetEntity);

		if (bot.HasNoAmmo())
			return EXIT;

		m_FireTimer -= pDt;
		if (m_FireTimer <= 0.0 && m_Target && m_Target.m_HasLOS)
		{
			pawn.RequestFire(m_TargetEntity);
			m_FireTimer = DM_BOT_FIRE_INTERVAL;
		}

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Shooting.exit");
		#endif

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetOwner().GetPawn());
		if (pawn)
			pawn.RaiseWeapon(false);
	}

	//! Resolve the current hostile target (nearest living threat).
	void ResolveTarget()
	{
		dmAISurvivor bot = GetOwner();
		m_Target = bot.GetHostileTarget();
		m_TargetEntity = null;
		if (m_Target)
			m_TargetEntity = m_Target.m_Entity;
	}
}
