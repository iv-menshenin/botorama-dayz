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
	vector m_EnemyVel;
	vector m_LastAimPos;
	bool m_EnemyPosKnown = false;

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
		m_EnemyVel = vector.Zero;
		m_LastAimPos = vector.Zero;
		m_EnemyPosKnown = false;

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

		//! Enemy velocity (smoothed per-axis) and the extrapolated aim point.
		//! First frame m_LastEnemyPos is zero — skip velocity to avoid a huge spike.
		vector aimPos = enemyPos;
		if (m_EnemyPosKnown)
		{
			vector instVel = enemyPos - m_LastEnemyPos;
			instVel[1] = 0.0;
			if (pDt > 0.0)
			{
				instVel[0] = instVel[0] / pDt;
				instVel[2] = instVel[2] / pDt;
			}
			m_EnemyVel[0] = m_EnemyVel[0] * 0.7 + instVel[0] * 0.3;
			m_EnemyVel[2] = m_EnemyVel[2] * 0.7 + instVel[2] * 0.3;
			aimPos = enemyPos + m_EnemyVel * DM_MELEE_EXTRAPOLATE_TIME;
			aimPos[1] = enemyPos[1];
		}
		m_EnemyPosKnown = true;
		m_LastEnemyPos = enemyPos;

		float reach = GetMeleeReach(bot);

		//! Movement: approach the extrapolated enemy position until within reach.
		//! Re-aim MoveTo when the aim point drifts more than 0.5 m.
		if (dist > reach)
		{
			vector aimDrift = aimPos - m_LastAimPos;
			aimDrift[1] = 0.0;
			if (m_Move && aimDrift.Length() > 0.5) { m_Move.Finish(); m_Move = null; }
			if (m_Move && (m_Move.IsFailed() || m_Move.IsFinished())) m_Move = null;
			if (!m_Move) { m_Move = new dmBotIntent_MoveTo(); m_Move.m_Target = aimPos; m_Move.m_ReachDistance = reach; bot.AddFSMIntent(m_Move); }
			m_LastAimPos = aimPos;
		}
		else
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
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
