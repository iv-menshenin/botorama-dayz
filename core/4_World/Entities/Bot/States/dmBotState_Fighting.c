//! dmBotState_Fighting — engage a hostile target in melee.
//!
//! PREEMPTIVE "melee fight" state (no raised stance — strikes come from ERECT).
//! A linear flow in OnUpdate (no explicit phases): approach the enemy, keep the
//! body turned toward it (HoldLook FULL), and strike on cooldown while in reach,
//! aligned to the target and with line of sight.
class dmBotState_Fighting : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_HoldLook m_Look;
	float m_Cooldown;
	vector m_LastEnemyPos;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		m_Look = null;
		m_Cooldown = 0.0;
		m_LastEnemyPos = vector.Zero;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Fighting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmTarget t = bot.GetHostileTarget();
		if (!t || !t.m_Entity)
			return EXIT;

		EntityAI enemy = t.m_Entity;
		if (!enemy.IsAlive())
			return EXIT;

		vector enemyPos = enemy.GetPosition();
		vector botPos = bot.GetPosition();
		vector toE = enemyPos - botPos;
		toE[1] = 0.0;
		float dist = toE.Length();

		float reach = GetMeleeReach(bot);

		//! Movement: approach until within weapon reach. Re-aim MoveTo when the
		//! target drifts more than ~1 m from the last aimed position.
		if (dist > reach)
		{
			vector drift = enemyPos - m_LastEnemyPos;
			drift[1] = 0.0;
			if (m_Move && drift.Length() > 1.0)
			{
				m_Move.Finish();
				m_Move = null;
			}

			if (m_Move && (m_Move.IsFailed() || m_Move.IsFinished()))
				m_Move = null;

			if (!m_Move)
			{
				m_Move = new dmBotIntent_MoveTo();
				m_Move.m_Target = enemyPos;
				m_Move.m_ReachDistance = reach;
				bot.AddFSMIntent(m_Move);
			}
			m_LastEnemyPos = enemyPos;
		}
		else
		{
			if (m_Move)
			{
				m_Move.Finish();
				m_Move = null;
			}
		}

		//! Face the enemy (FULL — body turned to the target) every tick.
		if (m_Look && (m_Look.IsFinished() || m_Look.IsExpired()))
			m_Look = null;
		if (!m_Look)
		{
			m_Look = new dmBotIntent_HoldLook();
			m_Look.m_Entity = enemy;
			m_Look.m_Turn = dmBotLookTurn.FULL;
			m_Look.m_Priority = dmBotIntentPriority.DESIRABLE;
			bot.AddFSMIntent(m_Look);
		}

		//! Strike on cooldown: in reach, body aligned, line of sight.
		m_Cooldown -= pDt;
		if (dist <= reach && m_Cooldown <= 0.0)
		{
			float yawTo = toE.VectorToAngles()[0];
			float bodyYaw = bot.GetOrientation()[0];
			float ang = dmAISurvivor.AngleDiff(yawTo, bodyYaw);

			if (Math.AbsFloat(ang) <= DM_MELEE_FACE_ANGLE && t.m_HasLOS)
			{
				dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
				if (pawn)
				{
					pawn.RequestMeleeAttack(enemy);
					m_Cooldown = DM_MELEE_COOLDOWN;

					#ifdef DM_BOT_DEBUG_FSM
					dmBotLog.Debug("[FSM] Fighting: удар по " + enemy.GetType());
					#endif
				}
			}
		}

		return CONTINUE;
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
		return DM_MELEE_REACH;
	}
}
