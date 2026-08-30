//! dmBotIntent_Approach — close the distance to a melee target.
//!
//! CRITICAL + PARALLEL. It finishes once the target is within m_ReachDistance.
//! Movement is delegated to an internal dmBotIntent_MoveTo (DESIRABLE) aimed at
//! the target's live position, re-created when the target drifts more than 0.5 m
//! from the current path goal. Close enough (<= DM_MELEE_APPROACH_NO_PATH_DIST)
//! it steers straight at the target without a navmesh path.
class dmBotIntent_Approach : dmBotIntent
{
	EntityAI m_TargetEntity;
	float m_ReachDistance = 1.5;

	ref dmBotIntent_MoveTo m_Move;

	void dmBotIntent_Approach()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (!m_TargetEntity)
		{
			Finish();
			return;
		}

		vector targetPos = m_TargetEntity.GetPosition();
		vector botPos = bot.GetPosition();
		vector d = targetPos - botPos;
		d[1] = 0.0;
		float dist = d.Length();

		if (dist <= m_ReachDistance)
		{
			Finish();
			return;
		}

		//! Close enough — steer straight at the target (no pathfinding).
		if (dist <= DM_MELEE_APPROACH_NO_PATH_DIST)
		{
			StopMove();
			float yaw = d.VectorToAngles()[0];
			bot.SetMoveYaw(yaw);
			bot.SetMove(0.0, 2.0);
			return;
		}

		if (m_Move && (m_Move.IsFailed() || m_Move.IsFinished()))
			m_Move = null;

		if (m_Move)
		{
			vector drift = targetPos - m_Move.m_Target;
			drift[1] = 0.0;
			if (drift.Length() > 0.5)
			{
				m_Move.Finish();
				m_Move = null;
			}
		}

		if (!m_Move)
		{
			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Target = targetPos;
			m_Move.m_ReachDistance = m_ReachDistance;
			m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
			bot.AddFSMIntent(m_Move);
		}
	}

	override void OnCancel(dmAISurvivor bot)
	{
		StopMove();
		bot.SetMove(0.0, 0.0);
	}

	void StopMove()
	{
		if (m_Move)
		{
			m_Move.Finish();
			m_Move = null;
		}
	}
}
