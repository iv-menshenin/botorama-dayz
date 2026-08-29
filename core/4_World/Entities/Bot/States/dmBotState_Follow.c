//! dmBotState_Follow — escort the bound player.
//!
//! On entry the bot picks a random side (left/right) and a random offset within
//! ~1 m of the player and walks to that "alongside" point (perpendicular to the
//! player's facing), so it stays side-by-side, never behind. Behaviour per tick:
//!  - player farther than DM_FOLLOW_FAR_DISTANCE -> catch up immediately (sprint,
//!    via the MoveTo deadline);
//!  - player moving (displacement over DM_FOLLOW_MOVE_EPS) -> follow at jog;
//!  - player standing/spinning nearby -> stand still for DM_FOLLOW_WAIT_TIME,
//!    then nudge back to the player.
//! A low-priority LookAround intent keeps the head scanning (NONE turn — the body
//! keeps facing the movement direction). Exits when the player is gone/dead.
class dmBotState_Follow : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_LookAround m_Look;
	vector m_LastTarget;

	float m_Side;            // +1 (right) / -1 (left)
	float m_SideDistance;    // random 0.5..1.0 meters
	vector m_LastPlayerPos;
	float m_MoveCheckTimer;
	bool m_PlayerMoving;
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
		m_LastTarget = vector.Zero;

		m_Side = 1.0;
		if (Math.RandomInt(0, 2) == 0)
			m_Side = -1.0;
		m_SideDistance = Math.RandomFloat(0.5, 1.0);

		PlayerBase player = GetOwner().GetFollowPlayer();
		m_LastPlayerPos = vector.Zero;
		if (player)
			m_LastPlayerPos = player.GetPosition();
		m_PlayerMoving = false;
		m_MoveCheckTimer = 0.0;
		m_WaitTimer = 0.0;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Follow.entry side=" + m_Side + " dist=" + m_SideDistance);
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

		//! Detect whether the player is moving (sample position periodically).
		m_MoveCheckTimer += pDt;
		if (m_MoveCheckTimer >= DM_FOLLOW_MOVE_CHECK_INTERVAL)
		{
			m_MoveCheckTimer = 0.0;
			vector dpp = player.GetPosition() - m_LastPlayerPos;
			dpp[1] = 0.0;
			m_PlayerMoving = dpp.Length() > DM_FOLLOW_MOVE_EPS;
			m_LastPlayerPos = player.GetPosition();
		}

		vector desired = FollowPosition(player);

		vector toPlayer = player.GetPosition() - bot.GetPosition();
		toPlayer[1] = 0.0;
		float distToPlayer = toPlayer.Length();

		vector toPoint = desired - bot.GetPosition();
		toPoint[1] = 0.0;
		float distToPoint = toPoint.Length();

		bool approach = false;
		float deadline = 0.0;

		if (distToPlayer > DM_FOLLOW_FAR_DISTANCE)
		{
			approach = true;
			deadline = distToPoint / DM_FOLLOW_CATCHUP_SPEED;
		}
		else if (m_PlayerMoving)
		{
			approach = true;
			m_WaitTimer = 0.0;
		}
		else
		{
			if (distToPoint > DM_FOLLOW_REACH_DISTANCE)
			{
				approach = true;
			}
			else
			{
				m_WaitTimer += pDt;
				if (m_WaitTimer >= DM_FOLLOW_WAIT_TIME)
				{
					m_WaitTimer = 0.0;
					approach = true;
				}
			}
		}

		if (!approach)
		{
			if (m_Move)
			{
				m_Move.Finish();
				m_Move = null;
			}
			return CONTINUE;
		}

		//! Manage the active MoveTo: replace when finished/failed or when the
		//! alongside point drifted beyond the re-target threshold.
		if (m_Move)
		{
			if (m_Move.IsFailed())
			{
				m_Move = null;
			}
			else if (m_Move.IsFinished())
			{
				m_Move = null;
			}
			else
			{
				vector drift = desired - m_LastTarget;
				drift[1] = 0.0;
				if (drift.Length() > DM_FOLLOW_RETARGET_DISTANCE)
				{
					m_Move.Finish();
					m_Move = null;
				}
			}
		}

		if (!m_Move && distToPoint > DM_FOLLOW_REACH_DISTANCE)
		{
			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Target = desired;
			m_Move.m_ReachDistance = DM_FOLLOW_REACH_DISTANCE;
			m_Move.m_ReachDeadline = deadline;
			m_LastTarget = desired;
			bot.AddFSMIntent(m_Move);
		}

		return CONTINUE;
	}

	//! The point beside the player (perpendicular to their facing, random side).
	vector FollowPosition(PlayerBase player)
	{
		vector pos = player.GetPosition();
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector side = Vector(-fwd[2], 0.0, fwd[0]);
		return pos + side * (m_Side * m_SideDistance * DM_FOLLOW_SIDE_DISTANCE);
	}
}
