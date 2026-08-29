//! dmBotState_Follow — escort the bound player: walk alongside them (not behind),
//! periodically scanning the surroundings with the head.
//!
//! Each tick it computes the "alongside" point (perpendicular to the player's
//! facing) and issues a MoveTo when the bot is out of reach. A low-priority
//! LookAround intent keeps the head scanning (NONE turn — the body keeps facing
//! the movement direction). Exits when the player is gone/dead.
class dmBotState_Follow : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_LookAround m_Look;
	vector m_LastTarget;

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

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Follow.entry");
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

		vector desired = FollowPosition(player);
		vector pos = bot.GetPosition();
		vector d = desired - pos;
		d[1] = 0.0;

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

		if (!m_Move && d.Length() > DM_FOLLOW_REACH_DISTANCE)
		{
			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Target = desired;
			m_Move.m_ReachDistance = DM_FOLLOW_REACH_DISTANCE;
			m_LastTarget = desired;
			bot.AddFSMIntent(m_Move);
		}

		return CONTINUE;
	}

	//! The point beside the player (perpendicular to their facing).
	vector FollowPosition(PlayerBase player)
	{
		vector pos = player.GetPosition();
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector side = Vector(-fwd[2], 0.0, fwd[0]);
		return pos + side * DM_FOLLOW_SIDE_DISTANCE;
	}
}
