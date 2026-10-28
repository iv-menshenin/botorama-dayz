//! dmBotIntent_Approach — close the distance to a melee target.
//!
//! CRITICAL + PARALLEL. It finishes once the target is within m_ReachDistance.
//! Movement is delegated to an internal dmBotIntent_MoveTo (DESIRABLE) aimed at
//! the target's live position, re-created when the target drifts more than 0.5 m
//! from the current path goal. Close enough (<= DM_MELEE_APPROACH_NO_PATH_DIST)
//! it steers straight at the target without a navmesh path.
class dmBotIntent_Approach: dmBotIntent_MoveTo
{
	EntityAI m_TargetEntity;

	float m_PathTimer;
	vector m_LastPathGoal = vector.Zero;
	bool m_PathGoalValid = false;

	void dmBotIntent_Approach()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "Approach";
	}

	override bool IsContinuous()
	{
		return true;
	}

	override bool KeepLookAtGoal()
	{
		return true;
	}

	override void OnStart(dmAISurvivor bot)
	{
		if ( m_TargetEntity ) m_Goal = m_TargetEntity.GetPosition();

		super.OnStart(bot);

		m_PathTimer = DM_FOLLOW_PATH_INTERVAL;
		m_LastPathGoal = vector.Zero;
		m_PathGoalValid = false;
	}

	override void UpdateGoal(dmAISurvivor bot, float pDt)
	{
		if ( m_TargetEntity ) m_Goal = m_TargetEntity.GetPosition();

		//! Ре-патинг по дрейфу цели (как FollowTo): маршрут перепрокладываем, только
		//! когда цель реально сдвинулась от конца прошлого пути (иначе ре-патинг
		//! сбрасывает progress-monitor и ломает stuck-detection).
		m_PathTimer += pDt;
		if (m_PathTimer >= DM_FOLLOW_PATH_INTERVAL)
		{
			m_PathTimer = 0.0;
			vector drift = m_Goal - m_LastPathGoal;
			drift[1] = 0.0;
			if (!m_PathGoalValid || drift.Length() > DM_APPROACH_REPATH_DIST)
			{
				m_LastPathGoal = m_Goal;
				m_PathGoalValid = true;
				RePath(bot);
			}
		}
	}
	
	override float GetMoveSpeed(dmAISurvivor bot)
	{
		return 3.0;
	}
}
