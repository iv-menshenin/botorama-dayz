//! dmBotState_Stealth — sneak to a cover position, then go prone and hide.
//!
//! Attribute: m_CoverPosition (the cover point). Flow:
//!   entry: crouch (stealth) + MoveTo cover -> reached: go prone -> dwell -> EXIT.
class dmBotState_Stealth : dmBotState
{
	vector m_CoverPosition;

	ref dmBotIntent_Stance m_StanceIntent;
	ref dmBotIntent_MoveTo m_Move;
	bool m_ReachedCover = false;
	float m_ProneTimer = 0.0;

	override bool CanEnter()
	{
		return m_CoverPosition != vector.Zero;
	}

	//! Stealth is a movement state — a future Fight (PREEMPTIVE) can evict it.
	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		m_ReachedCover = false;
		m_ProneTimer = 0.0;
		m_Move = null;
		m_StanceIntent = null;

		//! Crouch for the whole approach (stealth). No deadline: held until the
		//! FSM leaves this state (ClearFSMIntents on transition).
		m_StanceIntent = new dmBotIntent_Stance();
		m_StanceIntent.m_Stance = DayZPlayerConstants.STANCEIDX_CROUCH;
		m_StanceIntent.m_Priority = dmBotIntentPriority.CRITICAL;
		m_StanceIntent.m_Deadline = -1.0;
		GetOwner().AddFSMIntent(m_StanceIntent);

		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Target = m_CoverPosition;
		m_Move.m_ReachDistance = DM_PATROL_REACH_DISTANCE;
		GetOwner().AddFSMIntent(m_Move);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Stealth.entry cover=" + m_CoverPosition);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		if (!m_ReachedCover)
		{
			if (m_Move && m_Move.IsFailed())
			{
				dmBotLog.Error("[FSM] Stealth: укрытие " + m_CoverPosition + " недостижимо, выхожу");
				return EXIT;
			}

			if (m_Move && m_Move.IsFinished())
			{
				m_ReachedCover = true;
				if (m_StanceIntent)
					m_StanceIntent.m_Stance = DayZPlayerConstants.STANCEIDX_PRONE;

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Stealth reached cover, going prone");
				#endif
			}
			return CONTINUE;
		}

		m_ProneTimer += pDt;
		if (m_ProneTimer >= DM_STEALTH_PRONE_DWELL_TIME)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Stealth exit (dwell done)");
			#endif
			return EXIT;
		}
		return CONTINUE;
	}
}
