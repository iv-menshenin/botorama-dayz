//! dmBotState_Follow — escort the bound entity (player or other).
//!
//! The bot walks toward an anchor: the target's shoulder (±DM_FOLLOW_SIDE_DISTANCE
//! to the side) for a player/bot, or a point DM_FOLLOW_SIDE_DISTANCE short of an
//! item. It re-aims its MoveTo when the anchor drifts more than ~1 m, scales its
//! preferred speed by distance (sprint > DM_FOLLOW_SPRINT_DISTANCE, jog >
//! DM_FOLLOW_JOG_DISTANCE, walk otherwise), and exits once the target has stayed
//! put for DM_FOLLOW_EXIT_TIME (within DM_FOLLOW_EXIT_DISTANCE) with the bot in
//! place. A LookAround intent keeps the head scanning (NONE).
class dmBotState_Follow : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_LookAround m_Scan;
	float m_Threshold;
	float m_SideSign = 1.0;
	float m_SavedPreferredSpeed = 2.0;
	float m_ExitTimer;
	vector m_ExitRefPos;
	vector m_LastAnchorPos;
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
		m_LastAnchorPos = vector.Zero;
		m_SavedPreferredSpeed = GetOwner().GetPreferredSpeed();
		m_SideSign = 1.0;
		if (Math.RandomFloat01() < 0.5)
			m_SideSign = -1.0;
		CreateScan();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Follow.entry threshold=" + m_Threshold + " target=" + target);
		#endif
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

		if (bot.GetHostileTarget() != null)
			return EXIT;

		//! Horizontal distance bot -> target, computed once and reused below.
		vector botPos = bot.GetPosition();
		vector targetPos = target.GetPosition();
		vector toT = targetPos - botPos;
		toT[1] = 0.0;
		float distToTarget = toT.Length();

		//! Anchor: for a player/bot — the shoulder (±DM_FOLLOW_SIDE_DISTANCE to the
		//! side); for an item — DM_FOLLOW_SIDE_DISTANCE short of it along the approach.
		vector anchor = vector.Zero;
		if (player)
		{
			vector fwd = target.GetDirection();
			fwd[1] = 0.0;
			fwd.Normalize();
			vector side = Vector(-fwd[2], 0.0, fwd[0]);
			anchor = targetPos + side * (DM_FOLLOW_SIDE_DISTANCE * m_SideSign);
		}
		else
		{
			vector toItem = botPos - targetPos;
			toItem[1] = 0.0;
			toItem.Normalize();
			anchor = targetPos + toItem * DM_FOLLOW_SIDE_DISTANCE;
		}

		//! Horizontal distance bot -> anchor (used for movement + exit window).
		vector toA = anchor - botPos;
		toA[1] = 0.0;
		float distAnchor = toA.Length();

		//! Speed by distance to the target: sprint far, jog mid, walk near.
		if (distToTarget > DM_FOLLOW_SPRINT_DISTANCE)
			GetOwner().SetPreferredSpeed(3.0);
		else if (distToTarget > DM_FOLLOW_JOG_DISTANCE)
			GetOwner().SetPreferredSpeed(2.0);
		else
			GetOwner().SetPreferredSpeed(1.0);

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			dmBotLog.Debug("[FSM] Follow: dist=" + distToTarget + " anchor=" + distAnchor + " botPos=" + botPos);
			dmBotLog.Debug("[FSM] Follow: targetPos=" + targetPos + " targetType=" + target.GetType());
		}
		#endif

		//! Keep the head-scan intent alive (re-create if the pool dropped it).
		if (m_Scan && (m_Scan.IsFinished() || m_Scan.IsExpired()))
			m_Scan = null;
		if (!m_Scan)
			CreateScan();

		//! Exit window: reset the "target stood still" timer when the target moves
		//! farther than DM_FOLLOW_EXIT_DISTANCE from its reference point, or when
		//! the bot is still outside DM_FOLLOW_REACH of the anchor (has to catch up).
		//! EXIT only once the target is effectively motionless AND the bot is in
		//! place for DM_FOLLOW_EXIT_TIME seconds.
		vector d = targetPos - m_ExitRefPos;
		d[1] = 0.0;
		if (d.Length() > DM_FOLLOW_EXIT_DISTANCE || distAnchor > DM_FOLLOW_REACH)
		{
			m_ExitRefPos = targetPos;
			m_ExitTimer = 0.0;
		}
		else
		{
			m_ExitTimer += pDt;

			#ifdef DM_BOT_DEBUG_FSM
			m_ExitDebugAccum += pDt;
			if (m_ExitDebugAccum >= 1.0)
			{
				m_ExitDebugAccum = 0.0;
				dmBotLog.Debug("[FSM] Follow: exitWin dLen=" + d.Length() + " anchor=" + distAnchor + " exitTimer=" + m_ExitTimer);
			}
			#endif

			if (m_ExitTimer >= DM_FOLLOW_EXIT_TIME)
			{
				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Follow: цель стоит " + DM_FOLLOW_EXIT_TIME + "с, выход");
				#endif
				return EXIT;
			}
		}

		//! Movement: walk toward the anchor until within DM_FOLLOW_REACH.
		if (distAnchor > DM_FOLLOW_REACH)
		{
			vector drift = anchor - m_LastAnchorPos;
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
				m_Move.m_Target = anchor;
				m_Move.m_ReachDistance = DM_FOLLOW_REACH;

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Follow: MoveTo dist=" + distToTarget + " anchor=" + distAnchor + " target=" + m_Move.m_Target);
				#endif

				bot.AddFSMIntent(m_Move);
			}
			m_LastAnchorPos = anchor;
		}
		else
		{
			if (m_Move)
			{
				m_Move.Finish();
				m_Move = null;
			}
			m_LastAnchorPos = anchor;
		}

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		GetOwner().SetPreferredSpeed(m_SavedPreferredSpeed);
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
