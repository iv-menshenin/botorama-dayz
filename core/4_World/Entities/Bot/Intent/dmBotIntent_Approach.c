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
	}

	override void UpdateGoal(dmAISurvivor bot, float pDt)
	{
		if ( m_TargetEntity ) m_Goal = m_TargetEntity.GetPosition();
	}
	
	override float GetMoveSpeed(dmAISurvivor bot)
	{
		return 3.0;
	}
}
