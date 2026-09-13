//! dmBotState_Travel — кочёвка к ближайшему POI (напр. колодцу) при жажде.
//!
//! PREEMPTIVE: вытесняет INTERRUPTIBLE состояния (Exploration/Idle), когда бот
//! хочет пить и рядом есть застримленный колодец. Владеет MoveTo-интентом до
//! колодца; по прибытии/ошибке возвращает EXIT — следующий переход выбирает FSM.
class dmBotState_Travel : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;
	Building m_TargetBuilding;

	override dmBotStateKind GetKind() { return dmBotStateKind.PREEMPTIVE; }

	override bool CanEnter()
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;
		if (pawn.GetStatWater().Get() >= DM_TRAVEL_WATER_THRESHOLD)
			return false;
		Building w = dmLiveBuildingRegistry.Get().GetNearest(dmWorldPOIType.WATER, pawn.GetPosition(), DM_TRAVEL_POI_SEARCH_RADIUS);
		return w != null;
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		m_TargetBuilding = null;
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetOwner().GetPawn());
		if (!pawn)
			return;
		m_TargetBuilding = dmLiveBuildingRegistry.Get().GetNearest(dmWorldPOIType.WATER, pawn.GetPosition(), DM_TRAVEL_POI_SEARCH_RADIUS);
		if (m_TargetBuilding)
		{
			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Goal = m_TargetBuilding.GetPosition();
			m_Move.m_ReachDistance = DM_TRAVEL_REACH_DISTANCE;
			m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
			GetOwner().AddFSMIntent(m_Move);
		}

		#ifdef DM_BOT_DEBUG_FSM
		if (m_TargetBuilding)
			dmBotLog.Debug("[FSM] Travel.entry building=" + m_TargetBuilding.GetType() + " goal=" + m_Move.m_Goal);
		else
			dmBotLog.Debug("[FSM] Travel.entry building=none");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		if (!m_TargetBuilding)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Travel exit (no building)");
			#endif
			return EXIT;
		}

		//! Угроза: PREEMPTIVE не вытесняется боем, поэтому явно EXIT — FSM по
		//! приоритетным рёбрам (вес 2.0) уведёт в Fighting/Shooting.
		if (GetOwner().GetHostileTarget() != null)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Travel exit (threat)");
			#endif
			return EXIT;
		}

		if (!m_Move || m_Move.IsFinished() || m_Move.IsFailed() || m_Move.IsExpired())
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Travel exit (arrived/failed)");
			#endif
			return EXIT;
		}

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		m_TargetBuilding = null;
	}
}
