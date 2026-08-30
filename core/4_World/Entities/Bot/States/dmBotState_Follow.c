//! dmBotState_Follow — escort the bound entity (player or other).
//!
//! Two-intent model: when the target is visible or within the follow threshold the
//! bot runs a continuous dmBotIntent_FollowTo (which re-derives the escort anchor —
//! shoulder/side offset — and its speed from a 1 s extrapolation, pathfinding at
//! most 1 Hz). When the target is out of sight and farther than the threshold the
//! bot falls back to a dmBotIntent_MoveTo toward the target's last known position
//! (sprint via m_ReachDeadline) to catch up. If the target stays unseen for
//! DM_FOLLOW_LOST_SIGHT_TIME the bot refreshes only the last known position. A
//! LookAround intent keeps the head scanning (NONE). The state exits once the
//! target has stayed put for DM_FOLLOW_EXIT_TIME with the bot in place.
class dmBotState_Follow : dmBotState
{
	EntityAI m_TargetEntity;
	ref dmTarget m_Target;
	ref dmBotIntent_FollowTo m_IntentFollow;
	ref dmBotIntent_MoveTo m_IntentMove;
	ref dmBotIntent_LookAround m_Scan;
	float m_SideSign = 1.0;
	float m_LostSightTimer;
	float m_ExitTimer;
	vector m_ExitRefPos;
	float m_DebugAccum = 0.0;
	float m_ExitDebugAccum = 0.0;

	//! Follow-entry threshold by target kind: players may lead farther than others.
	static float GetThresholdDistance(EntityAI target)
	{
		if (PlayerBase.Cast(target) != null)
			return DM_FOLLOW_THRESHOLD_PLAYER;
		return DM_FOLLOW_THRESHOLD_OTHER;
	}

	override bool CanEnter()
	{
		dmAISurvivor bot = GetOwner();
		EntityAI target = bot.GetFollowTarget();
		if (!target)
			return false;
		dmTarget t = bot.FindTarget(target);
		if (!t)
			return false;
		if (t.m_LastPosition == vector.Zero)
			return false;
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
		m_TargetEntity = GetOwner().GetFollowTarget();
		m_Target = null;
		if (m_TargetEntity)
			m_Target = GetOwner().FindTarget(m_TargetEntity);
		m_IntentFollow = null;
		m_IntentMove = null;
		m_Scan = null;
		m_LostSightTimer = 0.0;
		m_ExitTimer = 0.0;
		m_ExitRefPos = vector.Zero;
		if (m_TargetEntity)
			m_ExitRefPos = m_TargetEntity.GetPosition();
		m_SideSign = 1.0;
		if (Math.RandomFloat01() < 0.5)
			m_SideSign = -1.0;
		CreateScan();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Follow.entry target=" + m_TargetEntity);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		m_TargetEntity = bot.GetFollowTarget();
		if (!m_TargetEntity)
			return EXIT;
		PlayerBase player = PlayerBase.Cast(m_TargetEntity);
		if (player && !player.IsAlive())
			return EXIT;
		if (bot.GetDefendTarget() != null)
			return EXIT;

		m_Target = bot.FindTarget(m_TargetEntity);
		if (!m_Target)
			return EXIT;

		//! «Магия»: цель не видна ≥ DM_FOLLOW_LOST_SIGHT_TIME → обновить последнюю
		//! известную позицию (только позицию), чтобы бот догонял актуальную точку.
		if (m_Target.m_HasLOS)
		{
			m_LostSightTimer = 0.0;
		}
		else
		{
			m_LostSightTimer += pDt;
			if (m_LostSightTimer >= DM_FOLLOW_LOST_SIGHT_TIME)
			{
				m_LostSightTimer = 0.0;
				m_Target.m_LastPosition = m_TargetEntity.GetPosition();
			}
		}

		//! Keep the head-scan intent alive (re-create if the pool dropped it).
		if (m_Scan && (m_Scan.IsFinished() || m_Scan.IsExpired()))
			m_Scan = null;
		if (!m_Scan)
			CreateScan();

		vector botPos = bot.GetPosition();
		vector targetPos = m_TargetEntity.GetPosition();
		vector toT = targetPos - botPos;
		toT[1] = 0.0;
		float dist = toT.Length();

		//! Выбор интента: цель видна ИЛИ близко (≤ порога) → FollowTo; иначе —
		//! MoveTo к последней известной позиции (догоняем спринтом).
		float threshold = GetThresholdDistance(m_TargetEntity);
		bool useFollow = m_Target.m_HasLOS || dist <= threshold;

		if (useFollow)
		{
			if (m_IntentMove) { m_IntentMove.Finish(); m_IntentMove = null; }
			if (m_IntentFollow && (m_IntentFollow.IsFinished() || m_IntentFollow.IsExpired()))
				m_IntentFollow = null;
			if (!m_IntentFollow)
			{
				m_IntentFollow = new dmBotIntent_FollowTo();
				m_IntentFollow.m_Target = m_TargetEntity;
				m_IntentFollow.m_SideSign = m_SideSign;
				bot.AddFSMIntent(m_IntentFollow);
			}
		}
		else
		{
		if (m_IntentFollow) { m_IntentFollow.Finish(); m_IntentFollow = null; }
		if (m_IntentMove && (m_IntentMove.IsFailed() || m_IntentMove.IsFinished()))
			m_IntentMove = null;
		if (m_IntentMove)
		{
			//! Re-aim if the last known position moved (the 1-min magic refreshed it).
			vector aimDrift = m_Target.m_LastPosition - m_IntentMove.m_Target;
			aimDrift[1] = 0.0;
			if (aimDrift.Length() > 1.0)
			{
				m_IntentMove.Finish();
				m_IntentMove = null;
			}
		}
		if (!m_IntentMove)
		{
			m_IntentMove = new dmBotIntent_MoveTo();
			m_IntentMove.m_Target = m_Target.m_LastPosition;
			m_IntentMove.m_ReachDistance = DM_FOLLOW_REACH;
			m_IntentMove.m_ReachDeadline = 1.0;
			bot.AddFSMIntent(m_IntentMove);
		}
		}

		//! Exit window: reset the "target stood still" timer when the target moves
		//! farther than DM_FOLLOW_EXIT_DISTANCE from its reference point, or when
		//! the bot is still outside reach+side of the target (has to catch up).
		//! EXIT only once the target is effectively motionless AND the bot is in
		//! place for DM_FOLLOW_EXIT_TIME seconds.
		vector d = targetPos - m_ExitRefPos;
		d[1] = 0.0;
		float inPlace = DM_FOLLOW_REACH + DM_FOLLOW_SIDE_DISTANCE;
		if (d.Length() > DM_FOLLOW_EXIT_DISTANCE || dist > inPlace)
		{
			m_ExitRefPos = targetPos;
			m_ExitTimer = 0.0;
		}
		else
		{
			m_ExitTimer += pDt;
			if (m_ExitTimer >= DM_FOLLOW_EXIT_TIME)
				return EXIT;
		}

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			dmBotLog.Debug("[FSM] Follow: dist=" + dist + " hasLOS=" + m_Target.m_HasLOS + " useFollow=" + useFollow);
		}
		#endif

		return CONTINUE;
	}

	//! Speed is no longer set via SetPreferredSpeed (the intents own it), and the
	//! intents are cleaned by ClearFSMIntents on transition — nothing to restore.
	override void OnExit(dmBotState to)
	{
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
