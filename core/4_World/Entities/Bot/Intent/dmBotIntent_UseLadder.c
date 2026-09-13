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
	float m_NoProgressTime = 0.0;   // время без вертикального прогресса (stuck-детект)
	float m_LastClimbY = 0.0;       // последний Y в фазе подъёма (для детекта прогресса)
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
		m_NoProgressTime = 0.0;
		m_LastClimbY = 0.0;
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
			vector flatDir = dir;
			flatDir[1] = 0.0;
			float dist = dir.Length();

			//! Точка направления входа/выхода (model-space → world): к ней разворачиваем
			//! корпус, чтобы нативная HumanCommandLadder.Exit() выпустила бота с верной
			//! стороны лестницы.
			vector dirPointModel = m_Ladder.m_BottomDir;
			if (m_Direction < 0)
				dirPointModel = m_Ladder.m_TopDir;
			vector dirPoint = m_Building.ModelToWorld(dirPointModel);

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

				float entryYaw = flatDir.VectorToAngles()[0];
				vector dirToPoint = dirPoint - pos;
				dirToPoint[1] = 0.0;
				float dirYaw = dirToPoint.VectorToAngles()[0];
				float bodyYaw = bot.GetOrientation()[0];
				float moveAngle = dmAISurvivor.AngleDiff(entryYaw, bodyYaw);
				bot.SetMoveYaw(dirYaw);
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
			m_LastClimbY = pawn.GetPosition()[1];

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Ladder] UseLadder: attach entry=" + m_Entry + " dirPoint=" + dirPoint + " dir=" + m_Direction);
			dmBotLog.Debug("[Ladder] UseLadder: attach orient=" + bot.GetOrientation()[0] + " index=" + m_Ladder.m_Index + " type=" + m_Ladder.m_Type);
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

		float curY = pawn.GetPosition()[1];
		if (Math.AbsFloat(curY - m_LastClimbY) > DM_LADDER_PROGRESS_EPS)
		{
			m_LastClimbY = curY;
			m_NoProgressTime = 0.0;
		}
		else
		{
			m_NoProgressTime += pDt;
		}

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
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Ladder] UseLadder: exit pos=" + bot.GetPosition() + " orient=" + bot.GetOrientation()[0]);
			#endif
			hcl.Exit();
			return;
		}

		//! Stuck: нет вертикального прогресса дольше порога. Сначала разворачиваемся
		//! (лезем обратно, НЕ слезая с лестницы), при повторном — сдаёмся (Exit).
		if (m_NoProgressTime > DM_LADDER_STUCK_TIME)
		{
			if (m_StuckReversals == 0)
			{
				m_Direction = -m_Direction;
				m_NoProgressTime = 0.0;
				m_LastClimbY = curY;
				m_StuckReversals = 1;

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[Ladder] UseLadder: stuck — reversing dir=" + m_Direction);
				#endif
			}
			else
			{
				if (hcl)
					hcl.Exit();

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
