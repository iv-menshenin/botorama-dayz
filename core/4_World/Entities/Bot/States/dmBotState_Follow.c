//! dmBotState_Follow — escort the bound entity (player or other).
//!
//! The bot walks toward the follow target until it is within DM_FOLLOW_REACH, then
//! stands. It re-aims its MoveTo when the target drifts more than ~1 m, catches up
//! (sprint, via the MoveTo deadline) when the target is beyond the threshold, and
//! exits once the target has stayed put for DM_FOLLOW_EXIT_TIME (within
//! DM_FOLLOW_EXIT_DISTANCE). A LookAround intent keeps the head scanning (NONE).
class dmBotState_Follow : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_LookAround m_Scan;
	float m_Threshold;
	float m_ExitTimer;
	vector m_ExitRefPos;
	vector m_LastTargetPos;

	//! Catch-up threshold by target kind: players may lead farther than others.
	static float GetThresholdDistance(EntityAI target)
	{
		if (PlayerBase.Cast(target) != null)
			return DM_FOLLOW_THRESHOLD_PLAYER;
		return DM_FOLLOW_THRESHOLD_OTHER;
	}

	override bool CanEnter()
	{
		return true;
	}

	//! Follow is a PREEMPTIVE state — it evicts an INTERRUPTIBLE Idle when the
	//! target gets far, so escorting takes priority over standing around.
	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		EntityAI target = GetOwner().GetFollowTarget();
		m_Threshold = GetThresholdDistance(target);
		m_ExitTimer = 0.0;
		m_ExitRefPos = vector.Zero;
		if (target)
			m_ExitRefPos = target.GetPosition();
		m_Move = null;
		m_LastTargetPos = vector.Zero;
		CreateScan();
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		EntityAI target = bot.GetFollowTarget();
		if (!target)
			return EXIT;

		PlayerBase player = PlayerBase.Cast(target);
		if (player && !player.IsAlive())
			return EXIT;

		//! Keep the head-scan intent alive (re-create if the pool dropped it).
		if (m_Scan && (m_Scan.IsFinished() || m_Scan.IsExpired()))
			m_Scan = null;
		if (!m_Scan)
			CreateScan();

		//! Exit window: if the target stays within DM_FOLLOW_EXIT_DISTANCE for
		//! DM_FOLLOW_EXIT_TIME seconds, the escort is done.
		vector d = target.GetPosition() - m_ExitRefPos;
		d[1] = 0.0;
		if (d.Length() > DM_FOLLOW_EXIT_DISTANCE)
		{
			m_ExitRefPos = target.GetPosition();
			m_ExitTimer = 0.0;
		}
		else
		{
			m_ExitTimer += pDt;
			if (m_ExitTimer >= DM_FOLLOW_EXIT_TIME)
				return EXIT;
		}

		//! Movement: walk toward the target until within DM_FOLLOW_REACH.
		vector toT = target.GetPosition() - bot.GetPosition();
		toT[1] = 0.0;
		float dist = toT.Length();

		if (dist > DM_FOLLOW_REACH)
		{
			vector drift = target.GetPosition() - m_LastTargetPos;
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
				float deadline = 0.0;
				if (dist > m_Threshold)
					deadline = dist / DM_FOLLOW_CATCHUP_SPEED;
				m_Move = new dmBotIntent_MoveTo();
				m_Move.m_Target = target.GetPosition();
				m_Move.m_ReachDistance = DM_FOLLOW_REACH;
				m_Move.m_ReachDeadline = deadline;
				bot.AddFSMIntent(m_Move);
			}
			m_LastTargetPos = target.GetPosition();
		}
		else
		{
			if (m_Move)
			{
				m_Move.Finish();
				m_Move = null;
			}
			m_LastTargetPos = target.GetPosition();
		}

		return CONTINUE;
	}

	void CreateScan()
	{
		m_Scan = new dmBotIntent_LookAround();
		m_Scan.m_AllowBodyTurn = false;
		m_Scan.m_Turn = dmBotLookTurn.NONE;
		m_Scan.m_Priority = dmBotIntentPriority.DESIRABLE;
		GetOwner().AddFSMIntent(m_Scan);
	}
}
