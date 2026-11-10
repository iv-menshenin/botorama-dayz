//! dmBotState_Travel — кочёвка между локациями (Nomad).
//! Вход: «делать больше нечего» ИЛИ бот вне какой-либо локации. Выбирает ближайшую
//! непосещённую локацию (dmExplorer.GetNearestUnvisitedLocation), бежит к ней
//! (прибытие = GetArrivalRadius типа). По прибытии — ArriveAtLocation + сброс флагов.
class dmBotState_Travel : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;

	override dmBotStateKind GetKind() { return dmBotStateKind.INTERRUPTIBLE; }

	override bool CanEnter()
	{
		dmAISurvivor bot = GetOwner();
		dmExplorer e = bot.GetExplorer();
		if (e.IsInTransit())
			return false;
		if (!e.IsNothingToDo() && dmWorldPOIRegistry.Get().GetLocationAt(bot.GetPosition()) != null)
			return false;
		return e.GetNearestUnvisitedLocation(bot) != null;
	}

	//! Выбрать следующую точку движения к `dest` и поставить MoveTo-интент.
	//! Если до локации дальше DM_TRAVEL_SEGMENT_DISTANCE — идём не сразу к центру,
	//! а к промежуточной точке в её направлении (нативный FindPath ограничен по
	//! веткам A* и не строит путь на 1.3+ км). По прибытии в конец куска OnUpdate
	//! либо фиксирует прибытие, либо ставит следующий кусок.
	void StartSegmentMove(dmAISurvivor bot, dmWorldPoiLocation dest)
	{
		vector botPos = bot.GetPosition();
		vector toPoi = dest.Position - botPos;
		toPoi[1] = 0.0;
		vector goal = dest.Position;
		if (toPoi.Length() > DM_TRAVEL_SEGMENT_DISTANCE)
		{
			toPoi.Normalize();
			goal = botPos + toPoi * DM_TRAVEL_SEGMENT_DISTANCE;
		}

		//! Снап к навмешу; недостижимую точку сдвигаем на DM_TRAVEL_SNAP_STEP ближе к
		//! боту, циклически, пока не найдётся навмеш (или не упрёмся в лимит/бота).
		vector sampled;
		bool snapped = false;
		int si;
		vector back;
		for (si = 0; si < DM_TRAVEL_SNAP_MAX_ATTEMPTS; si++)
		{
			if (bot.SampleNavmesh(goal, sampled))
			{
				goal = sampled;
				snapped = true;
				break;
			}
			back = botPos - goal;
			back[1] = 0.0;
			if (back.Length() < 0.5)
				break;                        // почти у бота — дальше не сдвинуть
			back.Normalize();
			goal = goal + back * DM_TRAVEL_SNAP_STEP;
		}
		if (!snapped)
			goal[1] = GetGame().SurfaceY(goal[0], goal[2]);   // фолбэк, если не вышло

		#ifdef DM_BOT_DEBUG_FSM
		if (si > 0)
			dmBotLog.Debug(bot.GetName(), "[FSM] Travel snap shifted " + si + " steps");
		#endif

		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = goal;
		m_Move.m_ReachDistance = dmWorldPOIRegistry.GetArrivalRadius(dest.Type);
		m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
		bot.AddFSMIntent(m_Move);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug(bot.GetName(), "[FSM] Travel.segment -> " + dest.Name + " goal=" + goal);
		#endif
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		dmAISurvivor bot = GetOwner();
		dmExplorer e = bot.GetExplorer();
		dmWorldPoiLocation dest = e.GetDestination();
		if (!dest)
			dest = e.GetNearestUnvisitedLocation(bot);
		if (!dest)
			return;

		e.SetDestination(dest);
		e.SetInTransit(true);

		StartSegmentMove(bot, dest);
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmExplorer e = bot.GetExplorer();

		if (bot.GetHostileTarget() != null)
			return EXIT;

		if (!m_Move)
			return EXIT;

		if (m_Move.IsFailed())
		{
			dmWorldPoiLocation dead = e.GetDestination();
			if (dead)
				e.RememberFailed(dead.Id);   // мягкий анти-цикл: кулдаун, не навсегда
			e.ClearDestination();
			e.SetInTransit(false);
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug(GetOwner().GetName(), "[FSM] Travel.failed, give up destination");
			#endif
			return EXIT;
		}

		if (m_Move.IsFinished())
		{
			dmWorldPoiLocation dest = e.GetDestination();
			if (!dest)
			{
				e.SetInTransit(false);
				return EXIT;
			}

			vector botPos = bot.GetPosition();
			vector toPoi = dest.Position - botPos;
			toPoi[1] = 0.0;
			float arrivalRadius = dmWorldPOIRegistry.GetArrivalRadius(dest.Type);
			if (toPoi.Length() <= arrivalRadius)
			{
				e.SetInTransit(false);
				e.ClearDestination();
				e.ArriveAtLocation(dest);
				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug(GetOwner().GetName(), "[FSM] Travel.arrived");
				#endif
				return EXIT;
			}

			//! Дошли до конца куска, но ещё не до локации — ставим следующий кусок.
			m_Move = null;
			StartSegmentMove(bot, dest);
			return CONTINUE;
		}

		if (m_Move.IsExpired())
		{
			e.ClearDestination();
			e.SetInTransit(false);   // ретрай позже (локация НЕ помечается посещённой)
			return EXIT;
		}

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		GetOwner().GetExplorer().SetInTransit(false);
	}
}
