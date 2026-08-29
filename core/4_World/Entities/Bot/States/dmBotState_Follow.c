//! dmBotState_Follow — escort the bound player.
//!
//! The bot picks a random side (left/right) and a random offset within ~1 m and
//! fixes a stand point beside the player (perpendicular to their current facing).
//! It only moves when it has to:
//!  - player farther than DM_FOLLOW_FAR_DISTANCE -> re-pick the side and catch up
//!    (sprint, via the MoveTo deadline);
//!  - not yet at the stand point -> walk there (jog);
//!  - settled and nearby -> stand; after DM_FOLLOW_WAIT_TIME re-pick the side and
//!    nudge back to the player.
//! Turning/shuffling within DM_FOLLOW_FAR_DISTANCE does NOT trigger movement —
//! the stand point is fixed until re-picked. A LookAround intent keeps the head
//! scanning (NONE turn). Exits when the player is gone/dead.
class dmBotState_Follow : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_LookAround m_Look;
	vector m_StandPoint;
	bool m_Settled;
	float m_WaitTimer;

	override bool CanEnter()
	{
		return GetOwner().GetFollowPlayer() != null;
	}

	//! Follow is a movement state — a future Fight (PREEMPTIVE) can evict it.
	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		m_Look = null;
		m_Settled = false;
		m_WaitTimer = 0.0;

		PlayerBase player = GetOwner().GetFollowPlayer();
		if (player)
			PickStandPoint(player);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Follow.entry standPoint=" + m_StandPoint);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		PlayerBase player = bot.GetFollowPlayer();
		if (!player || !player.IsAlive())
			return EXIT;

		//! Keep the head-scan intent alive (re-create if the pool dropped it).
		if (m_Look && (m_Look.IsFinished() || m_Look.IsExpired()))
			m_Look = null;
		if (!m_Look)
		{
			m_Look = new dmBotIntent_LookAround();
			m_Look.m_Interval = DM_FOLLOW_LOOK_INTERVAL;
			m_Look.m_YawRange = DM_FOLLOW_LOOK_RANGE;
			m_Look.m_Turn = dmBotLookTurn.NONE;
			m_Look.m_Priority = dmBotIntentPriority.DESIRABLE;
			bot.AddFSMIntent(m_Look);
		}

		vector toPlayer = player.GetPosition() - bot.GetPosition();
		toPlayer[1] = 0.0;
		float distToPlayer = toPlayer.Length();

		vector toPoint = m_StandPoint - bot.GetPosition();
		toPoint[1] = 0.0;
		float distToPoint = toPoint.Length();

		if (distToPlayer > DM_FOLLOW_FAR_DISTANCE)
		{
			PickStandPoint(player);
			m_Settled = false;
			m_WaitTimer = 0.0;

			toPoint = m_StandPoint - bot.GetPosition();
			toPoint[1] = 0.0;
			StartMove(bot, toPoint.Length() / DM_FOLLOW_CATCHUP_SPEED);
		}
		else if (!m_Settled)
		{
			if (m_Move && m_Move.IsFailed())
			{
				m_Move = null;
			}
			else if (m_Move && m_Move.IsFinished())
			{
				m_Move = null;
				m_Settled = true;
			}
			else if (!m_Move && distToPoint > DM_FOLLOW_REACH_DISTANCE)
			{
				StartMove(bot, 0.0);
			}
		}
		else
		{
			m_WaitTimer += pDt;
			if (m_WaitTimer >= DM_FOLLOW_WAIT_TIME)
			{
				PickStandPoint(player);
				m_Settled = false;
				m_WaitTimer = 0.0;

				StartMove(bot, 0.0);
			}
		}

		return CONTINUE;
	}

	//! Pick a random side (left/right) and a random offset, then fix the stand
	//! point beside the player (perpendicular to their current facing).
	void PickStandPoint(PlayerBase player)
	{
		float side = 1.0;
		if (Math.RandomInt(0, 2) == 0)
			side = -1.0;

		float dist = Math.RandomFloat(0.5, 1.0);

		vector pos = player.GetPosition();
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector perp = Vector(-fwd[2], 0.0, fwd[0]);

		m_StandPoint = pos + perp * (side * dist * DM_FOLLOW_SIDE_DISTANCE);
	}

	//! (Re)start the MoveTo toward the stand point with the given deadline
	//! (0 = no deadline, so CalcSpeed uses the preferred jog speed).
	void StartMove(dmAISurvivor bot, float deadline)
	{
		if (m_Move)
			m_Move.Finish();

		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Target = m_StandPoint;
		m_Move.m_ReachDistance = DM_FOLLOW_REACH_DISTANCE;
		m_Move.m_ReachDeadline = deadline;
		bot.AddFSMIntent(m_Move);
	}
}
