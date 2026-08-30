//! dmBotIntent_UseLadder — climb or descend a building ladder.
//!
//! EXCLUSIVE + CRITICAL: while it runs it owns the whole body — the ladder
//! command (HumanCommandLadder) takes over locomotion, so the MoveTo that spawned
//! it is dormant. Lifecycle: approach the entry point -> attach (StartCommand_Ladder)
//! -> climb/descend -> detach at the exit point (CanExit/Exit). The entry point is
//! a model-space point on the ladder (m_Bottom/m_Top) converted to world space via
//! Building.ModelToWorld. m_Direction is +1 up (enter at the bottom), -1 down
//! (enter at the top).
class dmBotIntent_UseLadder : dmBotIntent
{
	Building m_Building;
	ref dmBotLadder m_Ladder;
	int m_Direction;        // 1 = вверх (вход снизу), -1 = вниз (вход сверху)
	int m_Phase;            // 0 = approach, 1 = climb
	vector m_Entry;         // world-точка входа

	//! Grace period after attaching before IsClimbingLadder() is trusted — gives
	//! the ladder command a moment to become the active movement command.
	float m_AttachGrace = 0.0;

	void dmBotIntent_UseLadder()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
	}

	override void OnStart(dmAISurvivor bot)
	{
		m_Phase = 0;
		m_AttachGrace = 0.0;

		if (!m_Building || !m_Ladder)
		{
			Finish();
			return;
		}

		vector modelEntry = m_Ladder.m_Bottom;
		if (m_Direction < 0)
			modelEntry = m_Ladder.m_Top;
		m_Entry = m_Building.ModelToWorld(modelEntry);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Ladder] UseLadder.start building=" + m_Building + " dir=" + m_Direction + " entry=" + m_Entry);
		#endif
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());

		if (m_Phase == 0)
		{
			vector pos = bot.GetPosition();
			vector dir = m_Entry - pos;
			dir[1] = 0.0;
			float dist = dir.Length();

			if (dist > DM_LADDER_ATTACH_DIST)
			{
				float yaw = dir.VectorToAngles()[0];
				float bodyYaw = bot.GetOrientation()[0];
				float moveAngle = dmAISurvivor.AngleDiff(yaw, bodyYaw);
				bot.SetMoveYaw(yaw);
				bot.SetMove(moveAngle, 2.0);
				return;
			}

			if (!pawn)
			{
				Finish();
				return;
			}

			pawn.SetClimbingLadderType(m_Ladder.m_Type);
			pawn.StartCommand_Ladder(m_Building, m_Ladder.m_Index);
			m_Phase = 1;
			m_AttachGrace = DM_LADDER_ATTACH_GRACE;

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Ladder] UseLadder: attached index=" + m_Ladder.m_Index + " type=" + m_Ladder.m_Type);
			#endif
			return;
		}

		if (!pawn)
		{
			Finish();
			return;
		}

		if (m_AttachGrace > 0.0)
		{
			m_AttachGrace -= pDt;
			return;
		}

		if (!pawn.IsClimbingLadder())
		{
			Finish();
			return;
		}

		HumanCommandLadder hcl = pawn.GetCommand_Ladder();
		if (hcl && hcl.CanExit())
			hcl.Exit();
	}

	override void OnCancel(dmAISurvivor bot)
	{
		bot.SetMove(0.0, 0.0);
	}
}
