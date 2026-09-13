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
	float m_ClimbTime = 0.0;      // время в фазе подъёма (stuck-детект)
	float m_ApproachTime = 0.0;   // время в фазе подхода (timeout-детект)
	int m_StuckReversals = 0;     // сколько раз уже развернулись

	void dmBotIntent_UseLadder()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "UseLadder";
	}

	void UpdateEntry()
	{
		vector modelEntry = m_Ladder.m_Bottom;
		if (m_Direction < 0)
			modelEntry = m_Ladder.m_Top;
		m_Entry = m_Building.ModelToWorld(modelEntry);
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_Phase = 0;
		m_AttachGrace = 0.0;
		m_ClimbTime = 0.0;
		m_ApproachTime = 0.0;
		m_StuckReversals = 0;

		if (!m_Building || !m_Ladder)
		{
			Finish();
			return;
		}

		UpdateEntry();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Ladder] UseLadder.start building=" + m_Building + " dir=" + m_Direction + " entry=" + m_Entry);
		#endif
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());

		if (m_Phase == 0)
		{
			vector pos = bot.GetPosition();
			vector dir = m_Entry - pos;
			dir[1] = 0.0;
			float dist = dir.Length();

			if (dist > DM_LADDER_ATTACH_DIST)
			{
				m_ApproachTime += pDt;
				if (m_ApproachTime >= DM_LADDER_APPROACH_TIME)
				{
					#ifdef DM_BOT_DEBUG_FSM
					dmBotLog.Debug("[Ladder] UseLadder: approach timeout, giving up");
					#endif
					bot.SetMove(0.0, 0.0);
					Fail();
					return;
				}

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

		m_ClimbTime += pDt;

		if (!pawn.IsClimbingLadder())
		{
			Finish();
			return;
		}

		if ( m_Direction < 0 )
		{
			bot.SetMove(180.0, 3.0);
		} else {
			bot.SetMove(0.0, 2.0);
		}

		HumanCommandLadder hcl = pawn.GetCommand_Ladder();
		if (hcl && hcl.CanExit())
		{
			hcl.Exit();
			return;
		}

		//! Stuck: дольше порога не дошли до точки выхода. Разворачиваемся один раз
		//! (слезаем обратно и подходим с другого конца), при повторном — сдаёмся.
		if (m_ClimbTime > DM_LADDER_STUCK_TIME)
		{
			if (hcl)
				hcl.Exit();

			if (m_StuckReversals == 0)
			{
				m_Direction = -m_Direction;
				UpdateEntry();
				m_Phase = 0;
				m_ClimbTime = 0.0;
				m_ApproachTime = 0.0;
				m_AttachGrace = 0.0;
				m_StuckReversals = 1;

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[Ladder] UseLadder: stuck — reversing dir=" + m_Direction + " entry=" + m_Entry);
				#endif
			}
			else
			{
				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[Ladder] UseLadder: stuck again — giving up");
				#endif
				Fail();
			}
		}
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		bot.SetMove(0.0, 0.0);
	}
}
